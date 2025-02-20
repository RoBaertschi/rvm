#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <functional>
#include <iostream>
#include <ostream>
#include <rvm.hpp>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"
#include "rvm_terminal.hpp"
using namespace ftxui;


Component Wrap(std::string name, Component component) {
    return Renderer(
        component,
        [name, component] {
            return hbox({
                text(name) | center,
                component->Render() });
        }
    );
}

struct ConsoleUI {
    std::vector<std::string> instruction_kinds = {
        #define _X(kind, ...) #kind,
        INSTRUCTION_KIND
        #undef _X
    };
    std::vector<std::string> object_kinds = {
        #define _X(kind, ...) #kind,
        OBJECT_KIND
        #undef _X
    };

    int selected_instruction = 0;
    int selected_object = 0;
    int radio_inst_split = 30;
    int radio_obj_split = 60;

    std::string object_input = "0";
    rvm::u64 object_input_value = 0;

    bool is_object_input_invalid = false;

    std::vector<rvm::Instruction> instructions{};


    Component create_instruction() {
        return InstructionBuilder([this](rvm::Instruction i){
            instructions.push_back(i);
            rebuild_instruction_list();
        });
    }

    int selected = 0;
    std::vector<std::string> instruction_menu_entries{};

    int instruction_list_split = 30;

    void rebuild_instruction_list() {
        instruction_menu_entries.clear();
        for (rvm::u64 i = 0; i < instructions.size(); i++) {
            instruction_menu_entries.push_back(
                std::format("{} {}", i, instructions[i].string())
            );
        }
    }

    Component create_instruction_list() {
        rebuild_instruction_list();
        auto radiobox = Radiobox(&instruction_menu_entries, &selected, RadioboxOption::Simple());

        // Edit selected entry
        auto remove = Button("Remove", [this] {
            if (selected < 0 || static_cast<size_t>(selected) >= instructions.size()) { return; }

            instructions.erase(instructions.begin() + selected);
            rebuild_instruction_list();
        });

        // Menu
        auto container = Renderer(Container::Horizontal({radiobox, remove}), [=]{

            return hbox({
                radiobox->Render() | flex,
                separator(),
                vbox({ text("Edit"), remove->Render() | size(WIDTH, GREATER_THAN, 20) })
            });
        });

        return container;
    }

    std::string save_to_file{};
    std::string error_display{};
    Component create_save_to_file() {
        Component input = Input(&save_to_file);
        Component save_button = Button("Save", [this]{
            FILE *file = fopen(save_to_file.c_str(), "w+b");
            if (file == NULL) {
                error_display = std::format("Error while opening file: {}", strerror(errno));
                return;
            }
            defer(fclose(file));

            rvm::Error *error = nullptr;
            defer(if (error != nullptr) delete error;);

            for (auto instruction : instructions) {
                instruction.write(file, &error);

                if (error != nullptr) {
                    error_display = std::format("Error while writing instruction: {}", error->what());
                    delete error;
                    error = nullptr;
                    return;
                }
            }

        });

        return Renderer(
            Container::Horizontal({ input, save_button }),
            [=, this] {
                return hbox({input->Render() | size(WIDTH, GREATER_THAN, 30) | border, save_button->Render() | center, text(error_display) | color(Color::Red)});
            }
        );
    }

    bool show_quit_modal = false;
    Component quit_modal_component(ScreenInteractive *screen) {
        auto confirm_button = Button("Confirm", [screen] {
            screen->ExitLoopClosure()();
        });
        auto cancel_button = Button("Cancel", [this] {
            this->show_quit_modal = false;
        });

        return Renderer(Container::Horizontal({confirm_button, cancel_button}), [=] {
            return vbox({text("Are you sure? Did you save?") | center, hbox({confirm_button->Render() | flex_grow, cancel_button->Render() | flex_grow})}) | border;
        });
    }

    int Run() {
        auto screen = ScreenInteractive::TerminalOutput();
        auto component = create_instruction();
        auto instruction_list = create_instruction_list();
        auto save_to_file = create_save_to_file();
        auto renderer = Renderer(
            Container::Vertical({component, instruction_list, save_to_file }), [&] {
            return vbox({component->Render() | border, instruction_list->Render() | size(HEIGHT, GREATER_THAN, 1) | border, save_to_file->Render()});
        }) | Modal(quit_modal_component(&screen), &show_quit_modal);

        auto event_handler = CatchEvent(renderer, [&](Event event) {
            if (event == Event::Character('q')) {
                show_quit_modal = true;
                return true;
            }
            return false;
        });

        screen.Loop(event_handler);
        return 0;
    }
};

int main(int argc, char **argv) {
    std::vector<char*> args{argv, argv+argc};

    auto ui = ConsoleUI();

    if (argc > 1) {
        rvm::Error *error = nullptr;
        defer(if (error != nullptr) { delete error; });
        auto bytecode = rvm::bytecode_from_file(args[1], &error);
        if (error != nullptr) {
            std::cerr << "Error while parsing bytecode in file " << args[1] << " error: " << error->what() << std::endl;
            delete error;
            return 1;
        }

        ui.instructions = std::move(bytecode);
        ui.save_to_file = args[1];
    }

    return ui.Run();
}
