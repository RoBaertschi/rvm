#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/node.hpp"
#include "rvm.hpp"
#include "rvm_terminal.hpp"
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <vector>
using namespace ftxui;

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
            auto s = stack(vm);

            Add(
                Renderer(
                    [s, this] {
                        auto top = this->vm->stack.top();

                        return hbox({
                            vbox({
                                text(std::format("pc: {}", this->vm->pc)),
                                text(std::format("top: {}", top ? top->string() : "<none>")),
                            }) | border,
                            s->Render() | border | flex_grow,
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

    auto screen = ScreenInteractive::TerminalOutput();
    auto create_instruction = InstructionBuilder([=](rvm::Instruction i){
        vm->bytecode.push_back(i);
    });
    auto vms = vm_state(vm);
    screen.Loop(Container::Vertical({
        create_instruction,
        vms,
        Renderer([=] {
            std::vector<Element> elements{};

            for (size_t i = 0; i < vm->bytecode.size(); i++) {
                elements.push_back(text(std::format("{} - {}", i, vm->bytecode[i].string())));
            }

            return vbox(elements) | border | size(HEIGHT, GREATER_THAN, 0);
        }),
        Button("Tick", [=]{
            rvm::Error* error = nullptr;
            vm->tick(&error);

            if (error != nullptr) {
                *error_string = error->what();
                delete error;
            }
        }),
        Renderer([&]{ return text(*error_string) | color(Color::Red); }),
    }));
    return 0;
}
