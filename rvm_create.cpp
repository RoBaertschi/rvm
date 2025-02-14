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

    std::string object_input = "0";
    rvm::u64 object_input_value = 0;

    auto instructions_radio = Radiobox(&instructions, &selected_instruction);
    auto instructions_radio_renderer = Renderer(instructions_radio, [&] {
        return vbox({
            text("Instruction"),
            separator(),
            instructions_radio->Render(),
        });
    });
    auto object_radio = Radiobox(&object, &selected_object);
    auto object_input_field = Wrap("Input", Input(&object_input) | border);
    bool is_object_input_invalid = false;
    auto object_creation = Container::Horizontal({ object_radio, object_input_field });
    auto object_input_invalid = Renderer([&]{ return text("Only Numbers are valid!") | color(Color::Palette16::Red); }) | Maybe(&is_object_input_invalid);
    auto object_creation_renderer = Renderer(object_creation, [&] {
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
    auto object_create = Either(
        object_creation_renderer,
        Renderer([] { return text("Select a instruction kind with a object argument."); }),
        [&] {
            return static_cast<rvm::InstructionKind>(selected_instruction) == rvm::InstructionKind::Push;
        }
    );
    auto preview = Renderer([&] {
        return paragraph(
            std::format("{}{}",
                        instructions[selected_instruction],
                        static_cast<rvm::InstructionKind>(selected_instruction) == rvm::InstructionKind::Push
                        ? std::format(" {} {}", object[selected_object], object_input_value)
                        : ""
            )
        ) | center;
    });
    int radio_inst_split = 30;
    int radio_obj_split = 60;
    auto add_instruction = ResizableSplitLeft(
      instructions_radio_renderer,
      ResizableSplitLeft(object_create, preview, &radio_obj_split),
      &radio_inst_split);

    auto screen = ScreenInteractive::TerminalOutput();
    screen.Loop(add_instruction | border);
    return 0;
}
