# DTEditorLibrary - K2Node Sample Implementation

This plugin demonstrates how to create custom Blueprint nodes (K2Nodes) in Unreal Engine.

## Sample K2Node Implementation

The `UDTK2NodeTest` class shows how to implement a basic custom node for Blueprints. This sample node:

1. Creates input and output execution pins
2. Takes a string message input
3. Returns a boolean output
4. When executed, it prints the input message and returns true

## Key Components

### Header File (DTK2NodeTest.h)
- Defines the class inheriting from `UK2Node`
- Declares the necessary overrides from `UEdGraphNode` and `UK2Node` interfaces
- Adds customizable properties using UPROPERTY

### Implementation (DTK2NodeTest.cpp)
- `AllocateDefaultPins()` - Creates the node's pins (inputs/outputs)
- `GetNodeTitle()` - Sets the displayed name in the Blueprint editor
- `GetTooltipText()` - Provides the tooltip when hovering over the node
- `GetNodeTitleColor()` - Sets the color of the node
- `GetIconAndTint()` - Defines the icon displayed on the node
- `ExpandNode()` - The most important function; it defines what happens at compile time, replacing this node with actual functional nodes
- `GetMenuActions()` - Registers the node with the Blueprint editor's action database
- `GetMenuCategory()` - Defines the category where the node appears in the Blueprint context menu

## Node Expansion

When a Blueprint containing this node is compiled, the `ExpandNode()` method:
1. Creates a Print String node
2. Applies the customizable properties (print to screen setting and text color)
3. Creates a Make Literal Bool node (set to true)
4. Connects the input pins to the Print String node
5. Connects the Make Literal Bool output to our Success output
6. Removes the custom node, leaving only the functional nodes in the compiled Blueprint

## Customizable Properties

The node includes customizable properties that can be set in the Blueprint editor's details panel:

```cpp
// Custom properties for the node
UPROPERTY(EditAnywhere, Category = "Sample Node Settings")
bool bPrintToScreen = true;

UPROPERTY(EditAnywhere, Category = "Sample Node Settings")
FLinearColor PrintColor = FLinearColor(0.0f, 0.66f, 1.0f);
```

These properties affect the behavior of the node at compile time:
- `bPrintToScreen` - Controls whether messages are printed to the screen or just to the output log
- `PrintColor` - Sets the color of the printed text

During node expansion, these properties are applied to the generated Print String node:

```cpp
// Apply custom properties to the print string node
UEdGraphPin* PrintToPinPin = PrintStringNode->FindPinChecked(TEXT("bPrintToScreen"));
PrintToPinPin->DefaultValue = bPrintToScreen ? TEXT("true") : TEXT("false");

// Set the text color
UEdGraphPin* TextColorPin = PrintStringNode->FindPinChecked(TEXT("TextColor"));
FString ColorStr = PrintColor.ToString();
TextColorPin->DefaultValue = ColorStr;
```

## Blueprint Menu Registration

The node registers itself in the Blueprint editor menu using:

```cpp
void UDTK2NodeTest::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    UClass* ActionKey = GetClass();
    
    if (ActionRegistrar.IsOpenForRegistration(ActionKey))
    {
        UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
        check(NodeSpawner != nullptr);
        
        ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
    }
}

FText UDTK2NodeTest::GetMenuCategory() const
{
    return LOCTEXT("NodeCategory", "Custom|Sample Nodes");
}
```

This makes the node appear in the Blueprint editor under "Custom > Sample Nodes".

## Adding to Blueprints

To use this node in Blueprints:
1. Right-click in a Blueprint graph
2. Find the "Sample K2 Node" under the "Custom > Sample Nodes" category
3. Add it to your graph
4. Connect the execution pins and provide a message
5. Use the Success output as needed
6. Customize the node behavior by adjusting settings in the details panel

## Requirements

This implementation requires the following modules:
- BlueprintGraph
- UnrealEd
- KismetCompiler
- GraphEditor
- Kismet
- EditorStyle 