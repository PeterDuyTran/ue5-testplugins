// Fill out your copyright notice in the Description page of Project Settings.

#include "DTK2NodeTest.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeStruct.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EditorStyleSet.h"
#include "ScopedTransaction.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"

#define LOCTEXT_NAMESPACE "DTK2NodeTest"

void UDTK2NodeTest::AllocateDefaultPins()
{
    // Create the execution pins
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
    //
    // // Create a string parameter pin
    // CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, UEdGraphSchema_K2::PN_Index, TEXT("Message"));
    //
    // // Create an output value pin
    // CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, UEdGraphSchema_K2::PN_Index, TEXT("Success"));
}

FText UDTK2NodeTest::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "Sample K2 Node");
}

FText UDTK2NodeTest::GetTooltipText() const
{
    return LOCTEXT("NodeTooltip", "This is a sample K2 node that demonstrates how to create custom blueprint nodes");
}

FLinearColor UDTK2NodeTest::GetNodeTitleColor() const
{
    return FLinearColor(0.2f, 0.6f, 1.0f);
}

FSlateIcon UDTK2NodeTest::GetIconAndTint(FLinearColor& OutColor) const
{
    static FSlateIcon Icon("EditorStyle", "GraphEditor.Default_16x");
    OutColor = GetNodeTitleColor();
    return Icon;
}

void UDTK2NodeTest::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    // Get pins
    UEdGraphPin* ExecPin = GetExecPin();
    UEdGraphPin* ThenPin = GetThenPin();
    UEdGraphPin* MessagePin = FindPinChecked(TEXT("Message"));
    UEdGraphPin* SuccessPin = FindPinChecked(TEXT("Success"));
    
    if (ExecPin && ThenPin && MessagePin && SuccessPin)
    {
        // Create a Print String node
        UK2Node_CallFunction* PrintStringNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
        PrintStringNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString), UKismetSystemLibrary::StaticClass());
        PrintStringNode->AllocateDefaultPins();
        
        // Connect execution pins
        UEdGraphPin* PrintStringExecPin = PrintStringNode->GetExecPin();
        UEdGraphPin* PrintStringThenPin = PrintStringNode->GetThenPin();
        CompilerContext.MovePinLinksToIntermediate(*ExecPin, *PrintStringExecPin);
        CompilerContext.MovePinLinksToIntermediate(*ThenPin, *PrintStringThenPin);
        
        // Connect message pin
        UEdGraphPin* PrintStringTextPin = PrintStringNode->FindPinChecked(TEXT("InString"));
        CompilerContext.MovePinLinksToIntermediate(*MessagePin, *PrintStringTextPin);
        
        // Apply custom properties to the print string node
        UEdGraphPin* PrintToPinPin = PrintStringNode->FindPinChecked(TEXT("bPrintToScreen"));
        PrintToPinPin->DefaultValue = bPrintToScreen ? TEXT("true") : TEXT("false");
        
        // Set the text color
        UEdGraphPin* TextColorPin = PrintStringNode->FindPinChecked(TEXT("TextColor"));
        FString ColorStr = PrintColor.ToString();
        TextColorPin->DefaultValue = ColorStr;
        
        // Create a Make Literal Bool node for the success output
        UK2Node_CallFunction* MakeLiteralBoolNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
        MakeLiteralBoolNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralBool), UKismetSystemLibrary::StaticClass());
        MakeLiteralBoolNode->AllocateDefaultPins();
        
        // Set the literal value to true
        UEdGraphPin* ValuePin = MakeLiteralBoolNode->FindPinChecked(TEXT("Value"));
        ValuePin->DefaultValue = TEXT("true");
        
        // Connect the output of MakeLiteralBool to our Success pin
        UEdGraphPin* ReturnValuePin = MakeLiteralBoolNode->FindPinChecked(TEXT("ReturnValue"));
        CompilerContext.MovePinLinksToIntermediate(*SuccessPin, *ReturnValuePin);
        
        // Remove this node
        BreakAllNodeLinks();
    }
}

FNodeHandlingFunctor* UDTK2NodeTest::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
    return new FKCHandler_Passthru(CompilerContext);
}

void UDTK2NodeTest::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    UClass* ActionKey = GetClass();
    
    // Only register if the action hasn't already been registered
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

#undef LOCTEXT_NAMESPACE
