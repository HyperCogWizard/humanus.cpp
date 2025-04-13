#include "utils.h"

namespace humanus {

std::string get_update_memory_messages(const json& old_memories, const json& new_facts, const std::string& update_memory_prompt) {
    std::stringstream ss;
    ss << update_memory_prompt << "\n\n";
    ss << "Below is the current content of my memory which I have collected till now. You have to update it in the following format only:\n\n";
    ss << old_memories.dump(2) + "\n\n";
    ss << "The new retrieved facts are mentioned below. You have to analyze the new retrieved facts and determine whether these facts should be added, updated, or deleted in the memory.\n\n";
    ss <<  new_facts.dump(2) + "\n\n";
    ss << "You must return your response in the following JSON structure only:\n\n";
    ss << R"json({
    "memory" : [
        {
            "id" : <interger ID of the memory>,         # Use existing ID for updates/deletes, or new ID for additions
            "text" : "<Content of the memory>",         # Content of the memory
            "event" : "<Operation to be performed>",    # Must be "ADD", "UPDATE", "DELETE", or "NONE"
            "old_memory" : "<Old memory content>"       # Required only if the event is "UPDATE"
        },
        ...
    ]
})json" << "\n\n";
    ss << "Follow the instruction mentioned below:\n"
    << "- Do not return anything from the custom few shot prompts provided above.\n"
    << "- If the current memory is empty, then you have to add the new retrieved facts to the memory.\n"
    << "- You should return the updated memory in only JSON format as shown below. The memory key should be the same if no changes are made.\n"
    << "- If there is an addition, generate a new key and add the new memory corresponding to it.\n"
    << "- If there is a deletion, the memory key-value pair should be removed from the memory.\n"
    << "- If there is an update, the ID key should remain the same and only the value needs to be updated.\n"
    << "\n";
    ss << "Do not return anything except the JSON format.\n";
    return ss.str();
}

}