#ifndef HUMANUS_FLOW_FACTORY_H
#define HUMANUS_FLOW_FACTORY_H

#include "base.h"
#include "agent/base.h"
#include "planning.h"

namespace humanus {

// Factory for creating different types of flows with support for multiple agents
struct FlowFactory {
    template<typename... Args>
    static std::unique_ptr<BaseFlow> create_flow(FlowType flow_type, Args&&... args) {
        switch (flow_type) {
            case FlowType::PLANNING:
                return std::make_unique<PlanningFlow>(std::forward<Args>(args)...);
            default:
                throw std::invalid_argument("Unknown flow type: " + std::to_string(static_cast<int>(flow_type)));
        }
    }
};

}
#endif // HUMANUS_FLOW_FACTORY_H
