#pragma once

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "rvm.hpp"
#include <format>
#include <ftxui/component/event.hpp>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>

inline ftxui::Component Select(std::function<size_t()> show, std::vector<ftxui::Component> components) {
    class Impl : public ftxui::ComponentBase {
        std::function<size_t()> show_;
        std::vector<ftxui::Component> components_;

        ftxui::Element Render() override {
            auto to_show = show_();

            if (to_show >= components_.size()) {
                return ftxui::emptyElement();
            } else {
                return components_[to_show]->Render();
            }
        }

        bool Focusable() const override {
            auto to_show = show_();

            if (to_show >= components_.size()) {
                return false;
            } else {
                return components_[to_show]->Focusable();
            }
        }

        bool OnEvent(ftxui::Event event) override {
            auto to_show = show_();

            if (to_show >= components_.size()) {
                return false;
            } else {
                return components_[to_show]->OnEvent(event);
            }
        }

    public:
        Impl(std::function<size_t()> show, std::vector<ftxui::Component> components)
        : show_(show), components_(components)
        {}
    };

    return ftxui::Make<Impl>(std::move(show), std::move(components));
}

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

    // bool editor
    bool bool_editor_value = false;

    // u64 Editor
    std::string object_input = "0";
    bool is_object_input_invalid = false;
    rvm::u64 u64_editor_value = 0;

    // Final object data
    decltype(rvm::Object::data) object_input_value = static_cast<rvm::u64>(0);

    std::function<void(rvm::Instruction)> on_add;
public:
    InstructionBuilderBase(std::function<void(rvm::Instruction)> on_add) : on_add(std::move(on_add)) {
        auto instruction_kind_list = ftxui::Radiobox(&instruction_kinds, &selected_instruction);
        auto add_button = ftxui::Button("Add", [this] {
            rvm::InstructionKind kind = static_cast<rvm::InstructionKind>(selected_instruction);

            if (rvm::instruction_argument_amount(kind) == 0) {
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

        auto bool_check = ftxui::Checkbox("Value", &bool_editor_value);
        auto bool_editor = ftxui::Renderer(bool_check, [=, this] {
            this->object_input_value = this->bool_editor_value;
            return bool_check->Render();
        });

        auto u64_editor_input = ftxui::Input(&object_input) | ftxui::border;
        auto u64_editor_input_renderer = Renderer(
            u64_editor_input,
            [=] {
                return ftxui::hbox({
                    ftxui::text("Input") | ftxui::center,
                    u64_editor_input->Render(),
                });
            }
        );
        auto u64_editor_invalid = ftxui::Renderer([]{ return ftxui::text("Only Numbers are valid!") | ftxui::color(ftxui::Color::Red); }) | ftxui::Maybe(&is_object_input_invalid);
        auto u64_editor = ftxui::Renderer(
            u64_editor_input_renderer,
            [=, this] {
                rvm::u64 value = 0;
                auto result = std::from_chars(object_input.data(), object_input.data() + object_input.length(), value);
                is_object_input_invalid = result.ec != std::errc{};
                if (!is_object_input_invalid) {
                    object_input_value = value;
                    u64_editor_value = value;
                }

                return ftxui::vbox({
                    u64_editor_input_renderer->Render(),
                    u64_editor_invalid->Render() | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1),
                    ftxui::text(std::format("Result: {}", u64_editor_value)),
                });
            }
        );

        auto editor = Select([=, this] { return (size_t) selected_instruction; }, {
            u64_editor, u64_editor, bool_editor
        });

        auto object_module = Either(
            ftxui::Renderer(
                ftxui::Container::Horizontal({ object_kind_list, editor }),
                [=] {
                    return ftxui::vbox({
                    ftxui::text("Object Kind"),
                    ftxui::separator(),
                    ftxui::hbox({
                        object_kind_list->Render(),
                        ftxui::emptyElement() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 4),
                        editor->Render() | ftxui::flex,
                    }),
                });
            }),

            ftxui::Renderer([] {
                return ftxui::text("Select a instruction kind with a object argument.");
            }),
            [=, this] {
                return rvm::instruction_argument_amount(static_cast<rvm::InstructionKind>(selected_instruction)) > 0;
            }
        );

        auto preview_module = ftxui::Renderer(
            [=, this] {
                return ftxui::paragraph(
                    std::format(
                        "{}{}",
                        instruction_kinds[selected_instruction],
                        rvm::instruction_argument_amount(static_cast<rvm::InstructionKind>(selected_instruction)) > 0
                            ? std::format(" {}", rvm::Object(static_cast<rvm::ObjectKind>(selected_object), object_input_value).string())
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
