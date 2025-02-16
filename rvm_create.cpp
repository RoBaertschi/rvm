#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <format>
#include <functional>
#include <iostream>
#include <ostream>
#include <rvm.hpp>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"
using namespace ftxui;

Component Either(Component when_true, Component when_false, std::function<bool()> show) {
    class Impl : public ComponentBase {
    public:
        explicit Impl(std::function<bool()> show, Component when_true, Component when_false) : show_(std::move(show)), when_true_(std::move(when_true)), when_false_(std::move(when_false)) {}
    private:
        Element Render() override {
            return show_() ? when_true_->Render() : when_false_->Render();
        }

        bool Focusable() const override {
            return show_() ? when_true_->Focusable() : when_false_->Focusable();
        }

        bool OnEvent(Event event) override {
            return show_() ? when_true_->OnEvent(event) : when_false_->OnEvent(event);
        }

        std::function<bool()> show_;
        Component when_true_;
        Component when_false_;
    };

    return Make<Impl>(std::move(show), std::move(when_true), std::move(when_false));
}

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
        Component instructions_radio = Radiobox(&instruction_kinds, &selected_instruction);
        Component add_instruction = Button("Add", [=, this]{
            rvm::InstructionKind kind = static_cast<rvm::InstructionKind>(selected_instruction);

            if (kind != rvm::InstructionKind::Push) {
                instructions.push_back(
                    kind
                );
            } else {
                instructions.push_back(
                    rvm::Instruction{kind, new rvm::Object(static_cast<rvm::ObjectKind>(selected_object), object_input_value)}
                );
            }

            rebuild_instruction_list();
        });
        Component ins = Container::Vertical({instructions_radio, add_instruction});
        Component instructions_radio_renderer = Renderer(ins, [=] {
            return vbox({
                text("Instruction"),
                separator(),
                instructions_radio->Render(),
                add_instruction->Render(),
            });
        });

        Component object_radio = Radiobox(&object_kinds, &selected_object);
        Component object_input_field = Wrap("Input", Input(&object_input) | border);
        Component object_creation = Container::Horizontal({ object_radio, object_input_field });
        Component object_input_invalid = Renderer([]{ return text("Only Numbers are valid!") | color(Color::Palette16::Red); }) | Maybe(&is_object_input_invalid);
        Component object_creation_renderer = Renderer(
            object_creation,
            [=, this] {
            rvm::u64 value = 0;
            auto result = std::from_chars(object_input.data(), object_input.data() + object_input.length(), value);
            is_object_input_invalid = result.ec != std::errc{};
            if (!is_object_input_invalid) {
                object_input_value = value;
            }
            return vbox({
                text("Object Kind"),
                separator(),
                hbox({
                    object_radio->Render(),
                    emptyElement() | size(WIDTH, EQUAL, 4),
                    vbox({
                        object_input_field->Render(),
                        object_input_invalid->Render() | size(HEIGHT, GREATER_THAN, 1),
                        text(std::format("Result: {}", object_input_value))
                    }) | flex
                })
            });
        });
        Component object_create = Either(
            object_creation_renderer,
            Renderer([] { return text("Select a instruction kind with a object argument."); }),
            [=, this] {
                return static_cast<rvm::InstructionKind>(selected_instruction) == rvm::InstructionKind::Push;
            }
        );
        Component preview = Renderer([=, this] {
            return paragraph(
                std::format("{}{}",
                            instruction_kinds[selected_instruction],
                            static_cast<rvm::InstructionKind>(selected_instruction) == rvm::InstructionKind::Push
                            ? std::format(" {} {}", object_kinds[selected_object], object_input_value)
                            : ""
                )
            ) | center;
        });
        Component create_instruction = ResizableSplitLeft(
            instructions_radio_renderer,
            ResizableSplitLeft(object_create, preview, &radio_obj_split),
            &radio_inst_split
        );


        return create_instruction;
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
            if (selected < 0 || static_cast<size_t>(selected) >= instructions.size()) {return;}

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

            for (auto instruction : instructions) {
                instruction.write(file, &error);

                if (error != nullptr) {
                    error_display = std::format("Error while writing instruction: {}", error->what());
                    delete error;
                    error = nullptr;
                    return;
                }
            }

            defer(if (error != nullptr) delete error;);
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
