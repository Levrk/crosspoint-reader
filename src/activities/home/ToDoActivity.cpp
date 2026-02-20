#include "ToDoActivity.h"
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <HalDisplay.h>
#include "components/UITheme.h"
#include "fontIds.h"

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
    auto file = Storage.open("/todo.txt");
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
    auto file = Storage.open("/todo.txt", FILE_WRITE);
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
    // 1. Check input as the VERY first thing
    bool backPressed = mappedInput.wasReleased(MappedInputManager::Button::Back);
    bool confirmPressed = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
    bool upPressed = mappedInput.wasReleased(MappedInputManager::Button::Left); // Or Up
    bool downPressed = mappedInput.wasReleased(MappedInputManager::Button::Right); // Or Down

    if (backPressed) {
        onBack();
        return;
    }

    if (confirmPressed || upPressed || downPressed) {
        int actualIndex = visibleIndices[selectorIndex];

        if (upPressed) {
            selectorIndex = (selectorIndex > 0) ? selectorIndex - 1 : (int)visibleIndices.size() - 1;
        } 
        else if (downPressed) {
            selectorIndex = (selectorIndex < (int)visibleIndices.size() - 1) ? selectorIndex + 1 : 0;
        } 
        else if (confirmPressed) {
            auto& task = allTasks[actualIndex];
            if (task.isMajor) {
                task.isExpanded = !task.isExpanded;
                task.text[0] = task.isExpanded ? '-' : '+';
                rebuildVisibleTasks();
            } else {
                // Toggle logic...
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

    if (isDirty && (millis() - lastChangeTime > 3000)) {
        saveTasks();
        isDirty = false;
    }
}

void ToDoActivity::render() const {
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
        GUI.drawList(
            renderer, Rect{0, contentTop, pageWidth, contentHeight}, 
            visibleIndices.size(), selectorIndex,
            [this](int index) { 
                return allTasks[visibleIndices[index]].text; 
            }, nullptr, nullptr, nullptr);
    }

    const auto labels = mappedInput.mapLabels("Back", "Toggle", "Up", "Down");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(); 
}