#pragma once

#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>
#include <m/log.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

static constexpr DDCA_Vcp_Feature_Code DDC_FEAT_SOURCE = 0x60;

struct DDCDisplayId
{
  std::string model;
  std::string serial;

  inline bool operator<(const DDCDisplayId& o)  const { return std::tie(model, serial) <  std::tie(o.model, o.serial); }
  inline bool operator==(const DDCDisplayId& o) const { return std::tie(model, serial) == std::tie(o.model, o.serial); }
  inline friend std::ostream& operator<<(std::ostream& out, const DDCDisplayId& m)
  {
    out << "model=(" << m.model << ") serial=(" << m.serial << ")";
    return out;
  }
};

//TODO: Do all the DDC communication on a background thread, so as not to
// block the main XEvent thread.

////////////////////////////////////////////////////////////////////////////////
/// Manages display / monitor queries & commands using ddcutil library.
class DisplayDataChannel
{
  public:
    DisplayDataChannel() {}
    ~DisplayDataChannel();

    bool init(const std::vector<DDCDisplayId>& expected);
    bool poll();
    std::optional<uint8_t> getSource(const DDCDisplayId& id);
    bool setSource(const DDCDisplayId& id, uint8_t source);

  private:

    struct Display;

    DisplayDataChannel(const DisplayDataChannel& o) = delete;
    DisplayDataChannel& operator=(const DisplayDataChannel& o) = delete;
    DisplayDataChannel(DisplayDataChannel&& o) = delete;
    DisplayDataChannel& operator=(DisplayDataChannel&& o) = delete;

    std::map<DDCDisplayId,Display> _displays;
};

/// Implementation /////////////////////////////////////////////////////////////

struct DisplayDataChannel::Display
{
  DDCDisplayId id;
  DDCA_Display_Handle handle;
  std::optional<uint8_t> source;
};

inline DisplayDataChannel::~DisplayDataChannel()
{
  for (auto& [id,disp] : _displays)
    ddca_close_display(disp.handle);
  _displays.clear();
}

inline bool DisplayDataChannel::init(const std::vector<DDCDisplayId>& expected)
{
  ddca_reset_stats();

  ddca_enable_usb_display_detection(false);
  ddca_enable_report_ddc_errors(true);
  ddca_enable_verify(false);
  ddca_enable_error_info(true);
  ddca_enable_sleep_suppression(false);
  ddca_set_sleep_multiplier(3.0);
  ddca_set_max_tries(DDCA_WRITE_ONLY_TRIES, ddca_max_max_tries());
  ddca_set_max_tries(DDCA_WRITE_READ_TRIES, ddca_max_max_tries());
  ddca_set_max_tries(DDCA_MULTI_PART_TRIES, ddca_max_max_tries());
  LOG(INFO) << "ddc retry settings"
            << " max=" << ddca_max_max_tries()
            << " write_only=" << ddca_get_max_tries(DDCA_WRITE_ONLY_TRIES)
            << " write_read=" << ddca_get_max_tries(DDCA_WRITE_READ_TRIES)
            << " multi_part=" << ddca_get_max_tries(DDCA_MULTI_PART_TRIES);
  LOG(INFO) << "ddc sleep settings"
            << " suppression=" << (ddca_is_sleep_suppression_enabled() ? "true" : "false")
            << " multiplier=" << ddca_get_sleep_multiplier();

  DDCA_Display_Info_List* dlist = nullptr;
  if (ddca_get_display_info_list2(false, &dlist) != DDCRC_OK)
    return false;

  bool ret = true;
  for (int p = 0; p < dlist->ct; ++p) {
    const DDCA_Display_Info* dinfo = &dlist->info[p];

    const DDCDisplayId id{dinfo->model_name, dinfo->sn};

    if (_displays.find(id) != _displays.end()) {
      LOG(ERROR) << "ddc found duplicate display identifiers";
      ret = false;
      break;
    }

    if (std::find(expected.begin(), expected.end(), id) == expected.end()) {
      LOG(WARN) << "ddc ignoring display " << id;
      continue;
    }

    auto& disp = _displays.try_emplace(id, Display{ id, nullptr, std::nullopt }).first->second;

    if (ddca_open_display2(dinfo->dref, false, &(disp.handle)) != DDCRC_OK) {
      LOG(ERROR) << "ddc unable to open display " << id;
      ret = false;
      break;
    }

    LOG(INFO) << "ddc opened display " << id << " handle=(" << ddca_dh_repr(disp.handle) << ")";
  }
  ddca_free_display_info_list(dlist);

  //TODO: If we did not find a monitor in expected, return false

  return ret;
}

inline bool DisplayDataChannel::poll()
{
  for (auto& [id, disp] : _displays) {
    DDCA_Non_Table_Vcp_Value val;
    if (auto rc = ddca_get_non_table_vcp_value(disp.handle, DDC_FEAT_SOURCE, &val);
        rc != DDCRC_OK) {
      disp.source = std::nullopt;
      LOG(ERROR) << "ddc unable to query vcp value " << id
                 << " error=(" << ddca_rc_name(rc) << ")"
                 << " desc=(" << ddca_rc_desc(rc) << ")";
      //TODO: Print detailed error
      return false;
    }
    disp.source = val.sl;
    LOG(INFO) << "ddc queried input source vcp value " << id
              << " feat=(" << int(DDC_FEAT_SOURCE) << ")"
              << " val=(" << int(val.sl) << ")";
  }
  return true;
}

inline std::optional<uint8_t> DisplayDataChannel::getSource(const DDCDisplayId& id)
{
  auto it = _displays.find(id);
  if (it == _displays.end())
    return std::nullopt;
  return it->second.source;
}

inline bool DisplayDataChannel::setSource(const DDCDisplayId& id, uint8_t source)
{
  auto it = _displays.find(id);
  if (it == _displays.end())
    return false;
  auto& disp = it->second;

  if (auto rc = ddca_set_non_table_vcp_value(disp.handle, DDC_FEAT_SOURCE, 0x00, source);
      rc != DDCRC_OK) {
      disp.source = std::nullopt;
      LOG(ERROR) << "ddc unable to set vcp value " << id
                 << " error=(" << ddca_rc_name(rc) << ")"
                 << " desc=(" << ddca_rc_desc(rc) << ")";
      //TODO: Print detailed error
      return false;
  }
  LOG(INFO) << "ddc set input source vcp value " << id
            << " feat=(" << int(DDC_FEAT_SOURCE) << ")"
            << " val=(" << int(source) << ")";
  return true;
}
