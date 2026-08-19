#pragma once

#include <string>
#include <unordered_map>

namespace aw {

class ConfigFile {
public:
  ConfigFile() = default;

  bool load(const std::string& filepath);
  bool save(const std::string& filepath) const;

  std::string get_string(const std::string& section, const std::string& key, const std::string& default_val = "") const;
  int get_int(const std::string& section, const std::string& key, int default_val = 0) const;

  void set_string(const std::string& section, const std::string& key, const std::string& value);
  void set_int(const std::string& section, const std::string& key, int value);

private:
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data_;
};

}  // namespace aw
