#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <vector>
#include <string>
#include "../Activity.h"
#include "../../util/ButtonNavigator.h"

class ToDoActivity final : public Activity {
private:
    struct TaskItem {
    std::string text;
    bool isMajor = false;    // Starts with '>'
    bool isSubTask = false;  // Starts with tab/spaces
    bool isExpanded = false; // Only for Major tasks
    bool isChecked = false;  // [x] or {x}};
    };
    std::vector<TaskItem> allTasks; // The full list from file
    std::vector<int> visibleIndices; // Indices of tasks currently shown

    int selectorIndex = 0;
    bool updateRequired = false;
    const std::function<void()> onBack;
    bool isDirty = false;
    uint32_t lastChangeTime = 0;
    
    // Rendering task members (matching MyLibrary)
    TaskHandle_t displayTaskHandle = nullptr;
    SemaphoreHandle_t renderingMutex = nullptr;
    ButtonNavigator buttonNavigator;

    static void taskTrampoline(void* param);
    void displayTaskLoop();
    
    
    void saveTasks();
    void toggleTask(int index);
    
    void render() const;
    void rebuildVisibleTasks();   

    public:
    void loadTasks();
    void renderCustom(bool asleep = false) const;
    ToDoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onBack);
    void onEnter() override;
    void onExit() override;
    void loop() override;
};