#ifndef ROLE_MANAGER_H
#define ROLE_MANAGER_H

#include <mutex>
#include <string>
#include <vector>

struct RoleInfo {
  std::string name;
  std::string prompt;
  std::string response_handler; // e.g., "json_to_markdown"
};

class RoleManager {
public:
  static RoleManager &GetInstance();

  void LoadRoles();
  void SaveRoles();
  std::vector<RoleInfo> GetRoles();
  RoleInfo GetRole(const std::string &name);

  void AddRole(const RoleInfo &role);
  void UpdateRole(const std::string &originalName, const RoleInfo &newRole);
  void DeleteRole(const std::string &name);

  std::string GetConfigPath();
  std::string GetConfigDir();

private:
  RoleManager();
  // Delete copy constructor and assignment operator
  RoleManager(const RoleManager &) = delete;
  RoleManager &operator=(const RoleManager &) = delete;

  void InternalLoad();
  void InternalSave();
  void EnsureDefaultRoles();

  std::vector<RoleInfo> roles;
  std::mutex rolesMutex;
};

#endif
