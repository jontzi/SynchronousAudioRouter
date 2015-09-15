// SynchronousAudioRouter
// Copyright (C) 2015 Mackenzie Straight
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SynchronousAudioRouter.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "configui.h"
#include "dllmain.h"
#include "utility.h"

using namespace Sar;

PropertySheetPage::PropertySheetPage()
{
    ZeroMemory(static_cast<PROPSHEETPAGE *>(this), sizeof(PROPSHEETPAGE));
    dwSize = sizeof(PROPSHEETPAGE);
    hInstance = gDllModule;
    pfnDlgProc = &dialogProcStub;
    lParam = (LPARAM)this;
}

INT_PTR CALLBACK PropertySheetPage::dialogProcStub(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_INITDIALOG) {
        PropertySheetPage *page =
            (PropertySheetPage *)((PROPSHEETPAGE *)lparam)->lParam;

        page->_hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)page);
    }

    PropertySheetPage *page =
        (PropertySheetPage *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (page) {
        return page->dialogProc(hwnd, msg, wparam, lparam);
    }

    return 0;
}

PropertyDialog::PropertyDialog()
{
    ZeroMemory(static_cast<PROPSHEETHEADER *>(this), sizeof(PROPSHEETHEADER));
    dwSize = sizeof(PROPSHEETHEADER);
    dwFlags = PSH_DEFAULT;
    hInstance = gDllModule;
}

void PropertyDialog::addPage(std::shared_ptr<PropertySheetPage> page)
{
    HPROPSHEETPAGE handle = ::CreatePropertySheetPage(&*page);

    if (handle) {
        _pages.emplace_back(page);
        _pageHandles.emplace_back(handle);
    }
}

INT_PTR PropertyDialog::show(HWND parent)
{
    nPages = (UINT)_pages.size();
    phpage = _pageHandles.data();
    hwndParent = parent;

    return ::PropertySheet(this);
}

EndpointsPropertySheetPage::EndpointsPropertySheetPage(DriverConfig& config)
    : _config(config)
{
    pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_ENDPOINTS);

    for (auto driver : InstalledAsioDrivers()) {
        // Skip our own driver.
        if (driver.clsid != "{0569D852-1F6A-44A7-B7B5-EFB78B66BE21}") {
            _drivers.emplace_back(driver);
        }
    }
}

INT_PTR EndpointsPropertySheetPage::dialogProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_INITDIALOG:
            initControls();
            break;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case 1001:
                    if (HIWORD(wparam) == CBN_SELCHANGE) {
                        onHardwareInterfaceChanged();
                    }

                    break;
                case 1002:
                    if (HIWORD(wparam) == BN_CLICKED) {
                        onConfigureHardwareInterface();
                    }

                    break;
            }

            break;
    }

    return 0;
}

void EndpointsPropertySheetPage::onHardwareInterfaceChanged()
{
    auto index = ComboBox_GetCurSel(_hardwareInterfaceDropdown) - 1;

    if (index >= 0 && index < _drivers.size()) {
        _config.driverClsid = _drivers[index].clsid;
    } else {
        _config.driverClsid = "";
    }

    updateEnabled();
}

void EndpointsPropertySheetPage::onConfigureHardwareInterface()
{
    for (auto& driver : _drivers) {
        if (driver.clsid == _config.driverClsid) {
            CComPtr<IASIO> asio;

            if (SUCCEEDED(driver.open(&asio))) {
                // TODO: init driver
                asio->controlPanel();
            }

            break;
        }
    }
}

void EndpointsPropertySheetPage::initControls()
{
    _hardwareInterfaceDropdown = GetDlgItem(_hwnd, 1001);
    _hardwareInterfaceConfigButton = GetDlgItem(_hwnd, 1002);
    _listView = GetDlgItem(_hwnd, 1004);
    _addButton = GetDlgItem(_hwnd, 1005);
    _removeButton = GetDlgItem(_hwnd, 1006);

    ComboBox_AddString(_hardwareInterfaceDropdown, TEXT("None"));

    for (auto& driver : _drivers) {
        auto wstr = UTF8ToWide(driver.name);

        ComboBox_AddString(_hardwareInterfaceDropdown, wstr.c_str());
    }

    ComboBox_SetCurSel(_hardwareInterfaceDropdown, 0);
    updateEnabled();
}

void EndpointsPropertySheetPage::updateEnabled()
{
    Button_Enable(_hardwareInterfaceConfigButton,
        ComboBox_GetCurSel(_hardwareInterfaceDropdown) > 0);
}

ApplicationsPropertySheetPage::ApplicationsPropertySheetPage()
{
    pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_APPLICATIONS);
}

INT_PTR ApplicationsPropertySheetPage::dialogProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return 0;
}

ConfigurationPropertyDialog::ConfigurationPropertyDialog(DriverConfig& config)
    : _originalConfig(config),
      _newConfig(_originalConfig),
      _endpoints(std::make_shared<EndpointsPropertySheetPage>(_newConfig)),
      _applications(std::make_shared<ApplicationsPropertySheetPage>())
{
    pszCaption = TEXT("Synchronous Audio Router");
    addPage(_endpoints);
    addPage(_applications);
}