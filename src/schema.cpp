#include "schema.h"

namespace humanus {

std::map<AgentState, std::string> agent_state_map = {
    {AgentState::IDLE, "IDLE"},
    {AgentState::RUNNING, "RUNNING"},
    {AgentState::FINISHED, "FINISHED"},
    {AgentState::ERR, "ERROR"}
};

} // namespace humanus 