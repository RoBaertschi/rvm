#pragma once

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "rvm.hpp"
#include <format>
#include <functional>
#include <memory>
#include <string>

inline ftxui::Component Either(ftxui::Component when_true, ftxui::Component when_false, std::function<bool()> show) {
    class Impl : public ftxui::ComponentBase {
    public:
        explicit Impl(std::function<bool()> show, ftxui::Component when_true, ftxui::Component when_false) : show_(std::move(show)), when_true_(std::move(when_true)), when_false_(std::move(when_false)) {}
    private:
        ftxui::Element Render() override {
            return show_() ? when_true_->Render() : when_false_->Render();
        }

        bool Focusable() const override {
            return show_() ? when_true_->Focusable() : when_false_->Focusable();
        }

        bool OnEvent(ftxui::Event event) override {
            return show_() ? when_true_->OnEvent(event) : when_false_->OnEvent(event);
        }

        std::function<bool()> show_;
        ftxui::Component when_true_;
        ftxui::Component when_false_;
    };

    return Make<Impl>(std::move(show), std::move(when_true), std::move(when_false));
}

class InstructionBuilderBase : public ftxui::ComponentBase {
private:
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
    int left_module_split = 30;
    int right_module_split = 60;

    std::string object_input = "0";
    rvm::u64 object_input_value = 0;
    bool is_object_input_invalid = false;

    std::function<void(rvm::Instruction)> on_add;
public:
    InstructionBuilderBase(std::function<void(rvm::Instruction)> on_add) : on_add(std::move(on_add)) {
        auto instruction_kind_list = ftxui::Radiobox(&instruction_kinds, &selected_instruction);
        auto add_button = ftxui::Button("Add", [=, this] {
            rvm::InstructionKind kind = static_cast<rvm::InstructionKind>(selected_instruction);

            if (kind != rvm::InstructionKind::Push) {
                this->on_add(kind);
            } else {
                this->on_add(
                    rvm::Instruction{kind, new rvm::Object(static_cast<rvm::ObjectKind>(selected_object), object_input_value)}
                );
            }
        });

        auto left_module = ftxui::Renderer(ftxui::Container::Vertical({ instruction_kind_list, add_button }), [=] {
            return ftxui::vbox({
                ftxui::text("Instruction"),
                ftxui::separator(),
                instruction_kind_list->Render(),
                add_button->Render(),
            });
        });


        auto object_kind_list = ftxui::Radiobox(&object_kinds, &selected_object);
        auto object_data_input = ftxui::Input(&object_input) | ftxui::border;
        auto object_data_input_renderer = Renderer(
            object_data_input,
            [=] {
                return ftxui::hbox({
                    ftxui::text("Input") | ftxui::center,
                    object_data_input->Render(),
                });
            }
        );
        auto object_input_invalid = ftxui::Renderer([]{ return ftxui::text("Only Numbers are valid!") | ftxui::color(ftxui::Color::Red); }) | ftxui::Maybe(&is_object_input_invalid);
        auto object_editor = ftxui::Renderer(ftxui::Container::Horizontal({ object_kind_list, object_data_input_renderer }), [=, this] {
            rvm::u64 value = 0;
            auto result = std::from_chars(object_input.data(), object_input.data() + object_input.length(), value);
            is_object_input_invalid = result.ec != std::errc{};
            if (!is_object_input_invalid) {
                object_input_value = value;
            }

            return ftxui::vbox({
                ftxui::text("Object Kind"),
                ftxui::separator(),
                ftxui::hbox({
                    object_kind_list->Render(),
                    ftxui::emptyElement() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 4),
                    ftxui::vbox({
                        object_data_input_renderer->Render(),
                        object_input_invalid->Render() | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1),
                        ftxui::text(std::format("Result: {}", object_input_value)),
                    }) | ftxui::flex,
                }),
            });
        });
        auto object_module = Either(
            object_editor,
            ftxui::Renderer([] { return ftxui::text("Select a instruction kind with a object argument."); }),
            [=, this] {
                return static_cast<rvm::InstructionKind>(selected_instruction) == rvm::InstructionKind::Push;
            }
        );

        auto preview_module = ftxui::Renderer(
            [=, this] {
                return ftxui::paragraph(
                    std::format("{}{}",
                                instruction_kinds[selected_instruction],
                                static_cast<rvm::InstructionKind>(selected_instruction) == rvm::InstructionKind::Push
                                    ? std::format(" {} {}", object_kinds[selected_object], object_input_value)
                                    : ""
                                )
                );
            }
        );

        auto all = ftxui::ResizableSplitLeft(
            left_module,
            ftxui::ResizableSplitLeft(object_module, preview_module, &right_module_split),
            &left_module_split
        ) | ftxui::border;

        Add(all);
    }
};

inline ftxui::Component InstructionBuilder(std::function<void(rvm::Instruction)> on_add) {
    return ftxui::Make<InstructionBuilderBase>(std::move(on_add));
}

inline ftxui::Component QuitModal(std::string quit_message, bool *show, ftxui::ScreenInteractive *screen) {
    auto confirm_button = ftxui::Button("Confirm", [screen] {
        screen->ExitLoopClosure()();
    });
    auto cancel_button = ftxui::Button("Cancel", [show] {
        *show = false;
    });

    return ftxui::Renderer(
        ftxui::Container::Horizontal({confirm_button, cancel_button}),
        [confirm_button, cancel_button, quit_message] {
            return ftxui::vbox({
                ftxui::text(quit_message) | ftxui::center,
                ftxui::hbox({
                    confirm_button->Render() | ftxui::flex_grow,
                    cancel_button->Render() | ftxui::flex_grow,
                }),
            });
        }
    ) | ftxui::border;
}

inline ftxui::Component ConfirmQuit(ftxui::Component child, std::string quit_message, ftxui::ScreenInteractive *screen) {
    struct Impl : ftxui::ComponentBase {
        bool show = false;

        Impl(ftxui::Component child,
             std::string quit_message,
             ftxui::ScreenInteractive *screen) {
            auto event_handler = CatchEvent(
                child | Modal(
                    QuitModal(quit_message, &show, screen),
                    &show),
                [=, this](ftxui::Event event) {
                    if (event == ftxui::Event::Character('q')) {
                        show = true;
                        return true;
                    }
                    return false;
            });

            Add(event_handler);
        }
    };

    return ftxui::Make<Impl>(child, quit_message, screen);
}
