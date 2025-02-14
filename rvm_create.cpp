#include <rvm.hpp>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
using namespace ftxui;


int main() {
    int selected_instruction = 0;
    int selected_object = 0;

    std::vector<std::string> instructions = {
        #define _X(kind, ...) #kind,
        INSTRUCTION_KIND
        #undef _X
    };

    std::vector<std::string> object = {
        #define _X(kind, ...) #kind,
        OBJECT_KIND
        #undef _X
    };

    auto instructions_radio = Radiobox(&instructions, &selected_instruction);
    bool obj_enabled = false;
    auto object_radio = Maybe(Radiobox(&object, &selected_object), &obj_enabled);
    auto preview = Renderer([&] {
        return text(instructions[selected_instruction]) | center;
    });
    int radio_inst_split = 30;
    int radio_obj_split = 30;
    auto add_instruction = Renderer([&] {
        if (static_cast<rvm::InstructionKind>(selected_instruction) != rvm::InstructionKind::Push) {
            obj_enabled = true;
        } else {
            obj_enabled = false;
        }

        return ResizableSplitLeft(
            instructions_radio,
            ResizableSplitLeft(object_radio, preview, &radio_obj_split),
            &radio_inst_split
        )->Render();
    });

    auto screen = ScreenInteractive::TerminalOutput();
    screen.Loop(add_instruction | border);
    return 0;
}
