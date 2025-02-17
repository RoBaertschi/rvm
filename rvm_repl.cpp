#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"
#include "rvm.hpp"
#include "rvm_terminal.hpp"
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <vector>
using namespace ftxui;

// The ScrollerBase Component is from Arthur Sonzogni, the creator of ftxui.
// Copyright 2021 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found at https://github.com/ArthurSonzogni/git-tui/blob/master/LICENSE.
class ScrollerBase : public ComponentBase {
public:
    ScrollerBase(Component child) { Add(child); }

private:
    Element Render() final {
        auto focused = Focused() ? focus : ftxui::select;
        auto style = Focused() ? inverted : nothing;

        Element background = ComponentBase::Render();
        background->ComputeRequirement();
        size_ = background->requirement().min_y;
        return dbox({
            std::move(background),
            vbox({
                text(L"") | size(HEIGHT, EQUAL, selected_),
                text(L"") | style | focused,
            }),
        }) |
        vscroll_indicator | yframe | yflex | reflect(box_);
    }

    bool OnEvent(Event event) final {
        if (event.is_mouse() && box_.Contain(event.mouse().x, event.mouse().y))
            TakeFocus();

        int selected_old = selected_;
        if (event == Event::ArrowUp || event == Event::Character('k') ||
            (event.is_mouse() && event.mouse().button == Mouse::WheelUp)) {
            selected_--;
        }
        if ((event == Event::ArrowDown || event == Event::Character('j') ||
            (event.is_mouse() && event.mouse().button == Mouse::WheelDown))) {
            selected_++;
        }
        if (event == Event::PageDown)
            selected_ += box_.y_max - box_.y_min;
        if (event == Event::PageUp)
            selected_ -= box_.y_max - box_.y_min;
        if (event == Event::Home)
            selected_ = 0;
        if (event == Event::End)
            selected_ = size_;

        selected_ = std::max(0, std::min(size_ - 1, selected_));
        return selected_old != selected_;
    }

    bool Focusable() const final { return true; }

    int selected_ = 0;
    int size_ = 0;
    Box box_;
};

Component Scroller(Component child) {
  return Make<ScrollerBase>(std::move(child));
}

Component stack(std::shared_ptr<rvm::VM> vm) {
    struct Impl : ComponentBase {
        std::shared_ptr<rvm::VM> vm;
        Impl(std::shared_ptr<rvm::VM> vm) :vm(vm){}

        Element Render() override {
            std::vector<Element> elements{};
            elements.reserve(vm->stack.size());

            for (size_t i = 0; i < vm->stack.size(); i++) {
                elements.push_back(text(vm->stack[i].string()));
            }

            return vbox({ elements });
        }
    };

    return Make<Impl>(std::move(vm));
}

// |--------------|
// |  pc: 3       |
// |  top: U64 4  |
// |              |
// |--------------|

Component vm_state(std::shared_ptr<rvm::VM> vm) {
    struct Impl : ComponentBase {
        std::shared_ptr<rvm::VM> vm;
        Impl(std::shared_ptr<rvm::VM> vm) : vm(vm) {
            auto s = Scroller(stack(vm));

            Add(
                Renderer(s,
                    [s, this] {
                        auto top = this->vm->stack.top();

                        return hbox({
                            vbox({
                                text(std::format("pc: {}", this->vm->pc)),
                                text(std::format("top: {}", top ? top->string() : "<none>")),
                            }) | border,
                            s->Render() | flex_grow | border,
                        });
                    }
                ) | border
            );
        }
    };

    return Make<Impl>(std::move(vm));
}

int main() {
    std::shared_ptr<rvm::VM> vm = std::make_shared<rvm::VM>(std::vector<rvm::Instruction>());


    std::shared_ptr<std::string> error_string = std::make_shared<std::string>();

    auto screen = ScreenInteractive::Fullscreen();
    auto create_instruction = InstructionBuilder([=](rvm::Instruction i){
        vm->bytecode.push_back(i);
    });
    auto vms = vm_state(vm);
    screen.Loop(
        ConfirmQuit(
            Container::Vertical({
                create_instruction,
                Button("Tick", [=]{
                    rvm::Error* error = nullptr;
                    vm->tick(&error);

                    if (error != nullptr) {
                        *error_string = error->what();
                        delete error;
                    }
                }) | size(HEIGHT, GREATER_THAN, 0),
                Renderer([&]{ return text(*error_string) | color(Color::Red); }) | size(HEIGHT, GREATER_THAN, 0),
                vms | size(HEIGHT, EQUAL, 10),
                Scroller(Renderer([=] {
                    std::vector<Element> elements{};

                    for (size_t i = 0; i < vm->bytecode.size(); i++) {
                        elements.push_back(text(std::format("{} - {}", i, vm->bytecode[i].string())));
                    }

                    return vbox(elements) | size(HEIGHT, GREATER_THAN, 0);
                })) | border,
            }),
            "Are you sure? You will lose all of the VM state.",
            &screen
        )
        );
    return 0;
}
