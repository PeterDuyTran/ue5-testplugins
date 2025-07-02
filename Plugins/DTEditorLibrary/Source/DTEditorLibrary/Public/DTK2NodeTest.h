// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DTK2NodeTest.generated.h"

class FKismetCompilerContext;

/**
 * This is a sample K2Node implementation that demonstrates basic Blueprint node functionality.
 * K2Nodes are used to create custom nodes in Blueprint graphs that can perform various operations.
 */
UCLASS()
class DTEDITORLIBRARY_API UDTK2NodeTest : public UK2Node
{
	GENERATED_BODY()

public:
	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool IsNodePure() const override { return true; }
	virtual bool NodeCausesStructuralBlueprintChange() const override { return false; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	// End of UK2Node interface
	
	// Custom properties for the node
	UPROPERTY(EditAnywhere, Category = "Sample Node Settings")
	bool bPrintToScreen = true;
	
	UPROPERTY(EditAnywhere, Category = "Sample Node Settings")
	FLinearColor PrintColor = FLinearColor(0.0f, 0.66f, 1.0f);
};
