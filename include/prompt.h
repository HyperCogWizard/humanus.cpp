#ifndef HUMANUS_PROMPT_H
#define HUMANUS_PROMPT_H

namespace humanus {

namespace prompt {

namespace humanus {
extern const char* SYSTEM_PROMPT;
extern const char* NEXT_STEP_PROMPT;
} // namespace humanus

namespace planning {
extern const char* PLANNING_SYSTEM_PROMPT;
extern const char* NEXT_STEP_PROMPT;
} // namespace planning

namespace swe {
extern const char* SYSTEM_PROMPT;
extern const char* NEXT_STEP_TEMPLATE;
} // namespace swe

namespace toolcall {
extern const char* SYSTEM_PROMPT;
extern const char* NEXT_STEP_PROMPT;
extern const char* TOOL_HINT_TEMPLATE;
} // namespace toolcall

extern const char* FACT_EXTRACTION_PROMPT;
extern const char* UPDATE_MEMORY_PROMPT;

} // namespace prompt

} // namespace humanus

#endif // HUMANUS_PROMPT_H
