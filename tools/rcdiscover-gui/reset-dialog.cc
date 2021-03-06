/*
 * rcdiscover - the network discovery tool for rc_visard
 *
 * Copyright (c) 2017 Roboception GmbH
 * All rights reserved
 *
 * Author: Raphael Schaller
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// placed here to make sure to include winsock2.h before windows.h
#include "rcdiscover/wol.h"

#include "reset-dialog.h"

#include "event-ids.h"
#include "rcdiscover/utils.h"

#include "rcdiscover/wol_exception.h"
#include "rcdiscover/operation_not_permitted.h"

#include <thread>

#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/msgdlg.h>
#include <wx/valgen.h>
#include <wx/html/helpctrl.h>
#include <wx/cshelp.h>

ResetDialog::ResetDialog(wxHtmlHelpController *help_ctrl,
                         wxWindow *parent, wxWindowID id,
                         const wxPoint &pos,
                         long style,
                         const wxString &name) :
  wxDialog(parent, id, "Reset rc_visard", pos, wxSize(-1,-1), style, name),
  sensors_(nullptr),
  mac_{{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}},
  sensor_list_(nullptr),
  help_ctrl_(help_ctrl)
{
  auto *panel = new wxPanel(this, -1);
  auto *vbox = new wxBoxSizer(wxVERTICAL);

  auto *grid = new wxFlexGridSizer(2, 2, 10, 25);

  auto *sensors_text = new wxStaticText(panel, wxID_ANY, "rc_visard");
  grid->Add(sensors_text);

  auto *sensors_box = new wxBoxSizer(wxHORIZONTAL);
  sensors_ = new wxChoice(panel, ID_Sensor_Combobox);
  sensors_box->Add(sensors_, 1);
  grid->Add(sensors_box, 1, wxEXPAND);

  auto *mac_text = new wxStaticText(panel, wxID_ANY, "MAC address");
  grid->Add(mac_text);

  auto *mac_box = new wxBoxSizer(wxHORIZONTAL);
  int i = 0;
  for (auto& m : mac_)
  {
    if (i > 0)
    {
      mac_box->Add(new wxStaticText(panel, ID_MAC_Textbox, ":"));
    }
    m = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
                       wxDefaultPosition, wxSize(35, -1));
    mac_box->Add(m, 1);
    ++i;
  }
  grid->Add(mac_box, 1, wxEXPAND);

  vbox->Add(grid, 0, wxALL | wxEXPAND, 15);

  auto *button_box = new wxBoxSizer(wxHORIZONTAL);
  auto *reset_param_button = new wxButton(panel, ID_Reset_Params,
                                          "Reset Parameters", wxDefaultPosition,
                                          wxDefaultSize, wxBU_EXACTFIT);
  button_box->Add(reset_param_button, 0, wxEXPAND);
  auto *reset_gige_button = new wxButton(panel, ID_Reset_GigE,
                                         "Reset Network", wxDefaultPosition,
                                         wxDefaultSize, wxBU_EXACTFIT);
  button_box->Add(reset_gige_button, 0, wxEXPAND);
  auto *reset_all_button = new wxButton(panel, ID_Reset_All,
                                        "Reset All", wxDefaultPosition,
                                        wxDefaultSize, wxBU_EXACTFIT);
  button_box->Add(reset_all_button, 0, wxEXPAND);
  auto *switch_part_button = new wxButton(panel, ID_Switch_Partition,
                                          "Switch Partitions",
                                          wxDefaultPosition, wxDefaultSize,
                                          wxBU_EXACTFIT);
  button_box->Add(switch_part_button, 0, wxEXPAND);

  button_box->AddSpacer(20);
  int w, h;
  switch_part_button->GetSize(&w, &h);
  auto *help_button = new wxContextHelpButton(panel, ID_Help_Reset,
                                              wxDefaultPosition, wxSize(h,h));
  button_box->Add(help_button, 0, wxEXPAND);

  vbox->Add(button_box, 0, wxLEFT | wxRIGHT | wxBOTTOM, 15);

  panel->SetSizer(vbox);
  panel->Fit();
  Centre();

  this->Fit();

  Connect(ID_Sensor_Combobox,
          wxEVT_CHOICE,
          wxCommandEventHandler(ResetDialog::onSensorSelected));
  Connect(ID_Reset_Params,
          wxEVT_BUTTON,
          wxCommandEventHandler(ResetDialog::onResetButton));
  Connect(ID_Reset_GigE,
          wxEVT_BUTTON,
          wxCommandEventHandler(ResetDialog::onResetButton));
  Connect(ID_Reset_All,
          wxEVT_BUTTON,
          wxCommandEventHandler(ResetDialog::onResetButton));
  Connect(ID_Switch_Partition,
          wxEVT_BUTTON,
          wxCommandEventHandler(ResetDialog::onResetButton));
  Connect(ID_Help_Reset,
          wxEVT_BUTTON,
          wxCommandEventHandler(ResetDialog::onHelpButton));
}

void ResetDialog::setDiscoveredSensors(const wxDataViewListModel *sensor_list)
{
  sensor_list_ = sensor_list;

  if (sensor_list != nullptr)
  {
    sensors_->Clear();

    sensors_->Append("<Custom>");

    const auto rows = sensor_list->GetCount();
    for (typename std::decay<decltype(rows)>::type i = 0; i < rows; ++i)
    {
      wxVariant hostname{};
      wxVariant mac{};
      sensor_list->GetValueByRow(hostname, i, 0);
      sensor_list->GetValueByRow(mac, i, 3);
      const auto s = wxString::Format("%s - %s",
                                      hostname.GetString(),
                                      mac.GetString());
      sensors_->Append(s);
    }
  }

  clear();
}

void ResetDialog::setActiveSensor(const unsigned int row)
{
  sensors_->Select(static_cast<int>(row + 1));
  fillMac();
}

void ResetDialog::onSensorSelected(wxCommandEvent &)
{
  if (sensors_->GetSelection() != wxNOT_FOUND)
  {
    if (sensors_->GetSelection() == 0)
    {
      clear();
    }
    else
    {
      fillMac();
    }
  }
}

void ResetDialog::onResetButton(wxCommandEvent &event)
{
  try
  {
    std::string func_name("");
    uint8_t func_id(0);

    switch(event.GetId())
    {
      case ID_Reset_Params:
        {
          func_name = "reset parameters";
          func_id = 0xAA;
        }
        break;

      case ID_Reset_GigE:
        {
          func_name = "reset GigE";
          func_id = 0xBB;
        }
        break;

      case ID_Reset_All:
        {
          func_name = "reset all";
          func_id = 0xFF;
        }
        break;

      case ID_Switch_Partition:
        {
          func_name = "switch partition";
          func_id = 0xCC;
        }
        break;

      default:
        throw std::runtime_error("Unknown event ID");
    }

    std::array<uint8_t, 6> mac;
    std::string mac_string;

    for (uint8_t i = 0; i < 6; ++i)
    {
      const auto s = mac_[i]->GetValue().ToStdString();

      try
      {
        const auto v = std::stoul(s, nullptr, 16);
        if (v > 0xff)
        {
          throw std::invalid_argument("");
        }
        mac[i] = static_cast<uint8_t>(v);
      }
      catch(const std::invalid_argument&)
      {
        wxMessageBox(std::string("Each MAC address segment must contain ") +
                     "a hex value ranging from 0x00 to 0xff.",
                     "Error", wxOK | wxICON_ERROR);
        return;
      }

      if (i > 0)
      {
        mac_string += ":";
      }
      mac_string += s;
    }

    try
    {
      rcdiscover::WOL wol(mac, 9);

      std::ostringstream oss;
      oss << "Are you sure to " << func_name <<
             " of rc_visard with MAC-address " << mac_string << "?";
      const int answer = wxMessageBox(oss.str(), "", wxYES_NO);

      if (answer == wxYES)
      {
        wol.send({{0xEE, 0xEE, 0xEE, func_id}});
      }
    }
    catch(const rcdiscover::WOLException& ex)
    {
      wxMessageBox(ex.what(), "An error occurred", wxOK | wxICON_ERROR);
    }
  }
  catch(const rcdiscover::OperationNotPermitted&)
  {
    wxMessageBox(std::string("rcdiscovery probably requires root/admin ") +
                 "privileges for this operation.",
                 "Operation not permitted",
                 wxOK | wxICON_ERROR);
  }
}

void ResetDialog::onHelpButton(wxCommandEvent &)
{
  help_ctrl_->Display("help.htm#reset");

  // need second call otherwise it does not jump to "reset" section if the
  // help is displayed the first time
  help_ctrl_->Display("help.htm#reset");
}

void ResetDialog::clear()
{
  sensors_->SetSelection(0);

  for (uint8_t i = 0; i < 6; ++i)
  {
    mac_[i]->Clear();
    mac_[i]->SetEditable(true);
  }
}

void ResetDialog::fillMac()
{
  const int row = sensors_->GetSelection() - 1;

  if (row == wxNOT_FOUND)
  {
    return;
  }

  wxVariant mac_string{};
  sensor_list_->GetValueByRow(mac_string, static_cast<unsigned int>(row), 3);

  const auto mac = split<6>(mac_string.GetString().ToStdString(), ':');

  for (uint8_t i = 0; i < 6; ++i)
  {
    mac_[i]->ChangeValue(mac[i]);
    mac_[i]->SetEditable(false);
  }
}

BEGIN_EVENT_TABLE(ResetDialog, wxDialog)
END_EVENT_TABLE()
