#include "ToDoActivity.h"
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <HalDisplay.h>
#include "components/UITheme.h"
#include "fontIds.h"
#include "components/icons/uncheck.h"
#include "components/icons/open.h"
#include "components/icons/closed.h"
#include "components/icons/check24.h"

ToDoActivity::ToDoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onBack)
    : Activity("ToDo", renderer, mappedInput), onBack(onBack) {}

void ToDoActivity::onEnter() {
    Activity::onEnter();
    loadTasks();
    selectorIndex = 0;
    updateRequired = true;
}

void ToDoActivity::onExit() {
    Activity::onExit();
}

void ToDoActivity::loadTasks() {
    allTasks.clear();
    auto file = Storage.open("/todo.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (file) {
        std::string line;
        while (file.available()) {
            char c = file.read();
            if (c == '\n') {
                if (!line.empty()) {
                    TaskItem item;
                    item.text = line;
                    item.isMajor = (line[0] == '+' || line[0] == '-');
                    item.isSubTask = (line[0] == '\t');
                    item.isExpanded = (line[0] == '-'); 
                    allTasks.push_back(item);
                }
                line = "";
            } else if (c != '\r') {
                line += c;
            }
        }
        file.close();
    }
    rebuildVisibleTasks();
}

void ToDoActivity::saveTasks() {
    Storage.remove("/todo.txt"); 
    auto file = Storage.open("/todo.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (file) {
        for (const auto& t : allTasks) {
            file.println(t.text.c_str()); 
        }
        file.close();
    }
}

void ToDoActivity::rebuildVisibleTasks() {
    visibleIndices.clear();
    bool showSubTasks = false;

    for (int i = 0; i < (int)allTasks.size(); i++) {
        const auto& item = allTasks[i];
        if (item.isMajor) {
            visibleIndices.push_back(i);
            showSubTasks = item.isExpanded;
        } else if (item.isSubTask) {
            if (showSubTasks) visibleIndices.push_back(i);
        } else {
            visibleIndices.push_back(i);
            showSubTasks = false; 
        }
    }
    
    if (selectorIndex >= (int)visibleIndices.size() && !visibleIndices.empty()) {
        selectorIndex = visibleIndices.size() - 1;
    }
}

void ToDoActivity::loop() {
    bool backPressed = mappedInput.wasPressed(MappedInputManager::Button::Back);
    bool confirmPressed = mappedInput.wasPressed(MappedInputManager::Button::Confirm);
    bool leftPressed = mappedInput.wasPressed(MappedInputManager::Button::Left);
    bool rightPressed = mappedInput.wasPressed(MappedInputManager::Button::Right);
    bool upPressed = mappedInput.wasPressed(MappedInputManager::Button::Up);
    bool downPressed = mappedInput.wasPressed(MappedInputManager::Button::Down);

    if (backPressed) {
        onBack();
        return;
    }

    if (confirmPressed || leftPressed || rightPressed|| upPressed|| downPressed) {
        if (visibleIndices.empty()) return; // Safety check
        int actualIndex = visibleIndices[selectorIndex];

        if (leftPressed) {
            selectorIndex = (selectorIndex > 0) ? selectorIndex - 1 : (int)visibleIndices.size() - 1;
        } 
        else if (rightPressed) {
            selectorIndex = (selectorIndex < (int)visibleIndices.size() - 1) ? selectorIndex + 1 : 0;
        } 
        else if (upPressed) {
            // if not a subtask move current item to the top of the list along with any subtasks it contains
            if (!allTasks[actualIndex].isSubTask && actualIndex > 0) {
                // Find the end of this task's block (includes all its subtasks)
                int endIndex = actualIndex + 1;
                while (endIndex < (int)allTasks.size() && allTasks[endIndex].isSubTask) {
                    endIndex++;
                }
                
                // std::rotate gracefully shifts the block [actualIndex, endIndex) to the beginning (0)
                std::rotate(allTasks.begin(), allTasks.begin() + actualIndex, allTasks.begin() + endIndex);
                
                rebuildVisibleTasks();
                selectorIndex = 0; // The moved task is now the first item
                isDirty = true;
                lastChangeTime = millis();
            }
        } 
        else if (downPressed) {
            // if not a subtask move current item to the bottom of the list along with any subtasks it contains
            if (!allTasks[actualIndex].isSubTask) {
                // Find the end of this task's block (includes all its subtasks)
                int endIndex = actualIndex + 1;
                while (endIndex < (int)allTasks.size() && allTasks[endIndex].isSubTask) {
                    endIndex++;
                }
                
                if (endIndex < (int)allTasks.size()) { // Only move if it isn't already at the bottom
                    // std::rotate gracefully shifts the block [actualIndex, endIndex) to the end
                    std::rotate(allTasks.begin() + actualIndex, allTasks.begin() + endIndex, allTasks.end());
                    
                    rebuildVisibleTasks();
                    
                    // Find the new visible index so the cursor follows the moved task
                    int newActualIndex = allTasks.size() - (endIndex - actualIndex);
                    for (int i = 0; i < (int)visibleIndices.size(); i++) {
                        if (visibleIndices[i] == newActualIndex) {
                            selectorIndex = i;
                            break;
                        }
                    }
                    isDirty = true;
                    lastChangeTime = millis();
                }
            }
        }
        else if (confirmPressed) {
            auto& task = allTasks[actualIndex];
            if (task.isMajor) {
                task.isExpanded = !task.isExpanded;
                task.text[0] = task.isExpanded ? '-' : '+';
                rebuildVisibleTasks();
            } else {
                size_t markerPos = task.isSubTask ? 1 : 0;
                if (task.text.find("[ ]", markerPos) != std::string::npos) {
                    task.text.replace(task.text.find("[ ]", markerPos), 3, "[x]");
                } else {
                    task.text.replace(task.text.find("[x]", markerPos), 3, "[ ]");
                }
            }
            isDirty = true;
            lastChangeTime = millis();
        }
        updateRequired = true;
    }

    if (updateRequired) {
        render(); 
        updateRequired = false;
    }

    if (isDirty && (millis() - lastChangeTime > 2000)) {
        saveTasks();
        isDirty = false;
    }
}

void ToDoActivity::renderCustom(bool asleep) const {
    Activity::RenderLock lock(const_cast<ToDoActivity&>(*this));
    renderer.clearScreen();
    auto metrics = UITheme::getInstance().getMetrics();
    const auto pageWidth = renderer.getScreenWidth();
    const auto pageHeight = renderer.getScreenHeight();

    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "To-Do List");

    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

    if (visibleIndices.empty()) {
        renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, "No tasks found");
    } else {
        // We use the drawList version that takes an icon-getter function
        int effectiveSelectedIndex = asleep ? -1 : selectorIndex;
        GUI.drawList(
            renderer, 
            Rect{0, contentTop, pageWidth, contentHeight}, 
            (int)visibleIndices.size(), 
            effectiveSelectedIndex,
            [this](int index) { // Primary text
                std::string raw = allTasks[visibleIndices[index]].text;
                bool isSub = (raw[0] == '\t'); // Capture the tab state
                
                // Strip the markers
                if (raw.find("[x]") != std::string::npos) raw.replace(raw.find("[x]"), 3, "");
                else if (raw.find("[ ]") != std::string::npos) raw.replace(raw.find("[ ]"), 3, "");
                else if (raw[0] == '+' || raw[0] == '-') raw.erase(0, 1);
                
                // Return with the tab re-attached if it had one so the renderer can indent it
                return (isSub ? "\t" : "") + raw; 
            }, 
            nullptr, // <--- Tell drawList there is NO subtitle
            [this](int index) -> UIIcon { // The Icon Getter
                const auto& task = allTasks[visibleIndices[index]];
                if (task.isMajor) {
                    return (task.text[0] == '+') ? Closed : Open;
                }
                return (task.text.find("[x]") != std::string::npos) ? Check24 : Uncheck;
            }, 
            nullptr, // <--- Tell drawList there is NO additional right-side info
            false    // highlightValue flag
        );
    }

    if (!asleep) {
        const auto labels = mappedInput.mapLabels("Back", "Toggle", "Up", "Down");
        GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    }
    renderer.displayBuffer(); 
}

void ToDoActivity::render() const {
    renderCustom(false);
}