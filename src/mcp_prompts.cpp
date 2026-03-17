#include "mcp_prompts.h"

// Stub implementation -- TDD RED phase
// Tests should FAIL with these stubs

nlohmann::json get_all_prompts_json() {
    return nlohmann::json::array(); // Empty -- tests expect 4
}

nlohmann::json get_prompt_messages(const std::string& name, const nlohmann::json& arguments) {
    return nullptr;
}

bool prompt_exists(const std::string& name) {
    return false;
}
