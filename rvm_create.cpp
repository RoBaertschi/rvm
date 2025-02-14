#include <charconv>
#include <functional>
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
            std::cout << &instruction_kinds << selected_instruction;
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
        Component object_input_invalid = Renderer([=, this]{ return text("Only Numbers are valid!") | color(Color::Palette16::Red); }) | Maybe(&is_object_input_invalid);
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

    int Run() {
        auto screen = ScreenInteractive::TerminalOutput();
        auto component = create_instruction();
        screen.Loop(component | border);
        return 0;
    }
};

int main() {
    auto ui = ConsoleUI();
    return ui.Run();
}
