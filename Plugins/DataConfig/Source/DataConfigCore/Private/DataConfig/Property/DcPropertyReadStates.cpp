#include "DataConfig/Property/DcPropertyReadStates.h"
#include "DataConfig/DcTypes.h"
#include "DataConfig/DcEnv.h"
#include "DataConfig/Diagnostic/DcDiagnosticCommon.h"
#include "DataConfig/Diagnostic/DcDiagnosticReadWrite.h"
#include "DataConfig/Property/DcPropertyReader.h"
#include "DataConfig/SerDe/DcSerDeCommon.h"

#if !UE_VERSION_OLDER_THAN(5, 4, 0)
#include "UObject/PropertyOptional.h"
#endif // !UE_VERSION_OLDER_THAN(5, 4, 0)

namespace DcPropertyReadStateDetails
{

static FORCEINLINE FDcResult CheckExpectedProperty(FDcPropertyReader* Parent, FField* Property, FFieldClass* ExpectedPropertyClass)
{
	if (!Property->IsA(ExpectedPropertyClass))
		return DC_FAIL(DcDReadWrite, PropertyMismatch)
		<< ExpectedPropertyClass->GetFName() << Property->GetFName() << Property->GetClass()->GetFName()
		<< Parent->FormatHighlight();
	else
		return DcOk();
}

static FORCEINLINE FDcResult HeuristicCheckStatesTooDeep(FDcPropertyReader* Parent)
{
	if (Parent->States.Num() > DC_HEURISTIC_MAX_PROPERTIES_DEPTH)
		return DC_FAIL(DcDReadWrite, HeuristicPropertiesTooDeep);

	return DcOk();
}

}	// namespace DcPropertyReadStateDetails


FDcResult FDcBaseReadState::PeekRead(FDcPropertyReader* Parent, EDcDataEntry*) { return DC_FAIL(DcDCommon, NotImplemented); }
FDcResult FDcBaseReadState::ReadName(FDcPropertyReader* Parent, FName*) { return DC_FAIL(DcDCommon, NotImplemented); }
FDcResult FDcBaseReadState::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass*, FDcPropertyDatum&) { return DC_FAIL(DcDCommon, NotImplemented); }
FDcResult FDcBaseReadState::SkipRead(FDcPropertyReader* Parent) { return DC_FAIL(DcDCommon, NotImplemented); }
FDcResult FDcBaseReadState::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty) { return DC_FAIL(DcDCommon, NotImplemented); }
FDcResult FDcBaseReadState::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr) { return DC_FAIL(DcDCommon, NotImplemented); }

EDcPropertyReadType FDcReadStateNone::GetType()
{
	return EDcPropertyReadType::None;
}

FDcResult FDcReadStateNone::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	ReadOut(OutPtr, EDcDataEntry::Ended);
	return DcOk();
}

void FDcReadStateNone::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	DcPropertyHighlight::FormatNone(OutSegments, SegType);
}

EDcPropertyReadType FDcReadStateClass::GetType()
{
	return EDcPropertyReadType::ClassProperty;
}

FDcResult FDcReadStateClass::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	if (State == EState::ExpectRoot)
	{
		ReadOut(OutPtr, EDcDataEntry::ClassRoot);
		return DcOk();
	}
	else if (State == EState::ExpectEnd)
	{
		ReadOut(OutPtr,EDcDataEntry::ClassEnd);
		return DcOk();
	}
	else if (State == EState::ExpectNone)
	{
		ReadOut(OutPtr, EDcDataEntry::None);
		return DcOk();
	}
	else if (State == EState::ExpectReference)
	{
		ReadOut(OutPtr, EDcDataEntry::ObjectReference);
		return DcOk();
	}
	else if (State == EState::ExpectKey)
	{
		ReadOut(OutPtr, EDcDataEntry::Name);
		return DcOk();
	}
	else if (State == EState::ExpectValue)
	{
		check(Property);
		if (DcPropertyUtils::IsScalarArray(Property))
			return ReadOutOk(OutPtr, EDcDataEntry::ArrayRoot);
		else
			return ReadOutOk(OutPtr, DcPropertyUtils::PropertyToDataEntry(Property));
	}
	else
	{
		return DcNoEntry();
	}
}

FDcResult FDcReadStateClass::ReadName(FDcPropertyReader* Parent, FName* OutNamePtr)
{
	if (State == EState::ExpectKey)
	{
		check(Property);
		ReadOut(OutNamePtr, Property->GetFName());

		State = EState::ExpectValue;
		return DcOk();
	}
	else if (State == EState::ExpectValue)
	{
		FDcPropertyDatum Datum;
		DC_TRY(ReadDataEntry(Parent, FNameProperty::StaticClass(), Datum));

		ReadOut(OutNamePtr, Datum.CastFieldChecked<FNameProperty>()->GetPropertyValue(Datum.DataPtr));
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< (int)State << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateClass::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass* ExpectedPropertyClass, FDcPropertyDatum& OutDatum)
{
	if (State == EState::ExpectKey)
	{
		// should've be already handled in `ReadName`
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectValue << (int)State
			<< Parent->FormatHighlight();
	}
	else if (State == EState::ExpectValue)
	{
		check(ExpectedPropertyClass);
		check(Property);

		DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, Property, ExpectedPropertyClass));

		OutDatum.Property = Property;
		OutDatum.DataPtr = Property->ContainerPtrToValuePtr<void>(ClassObject);

		EndValueRead(Parent);
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectValue << (int)State
			<< Parent->FormatHighlight();
	}
}

void FDcReadStateClass::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	DcPropertyHighlight::FormatClass(OutSegments, SegType, ObjectName, Class, Property);
}

FDcResult FDcReadStateClass::ReadClassRootAccess(FDcPropertyReader* Parent, FDcClassAccess& Access)
{
	DC_TRY(DcPropertyReadStateDetails::HeuristicCheckStatesTooDeep(Parent));
	DC_TRY(DcPropertyUtils::HeuristicVerifyPointer(ClassObject));
	if (State == EState::ExpectRoot)
	{
		if (ClassObject == nullptr)
		{
			check(Type == EType::PropertyNormal || Type == EType::PropertyInstanced);

			Access.Name = Class->GetFName();
			Access.Control = FDcClassAccess::EControl::ReferenceOrNone;

			State = EState::ExpectNone;
			return DcOk();
		}
		else
		{
			if (Type == EType::PropertyNormal)
			{
				State = EState::ExpectReference;

				Access.Name = Class->GetFName();
				Access.Control = FDcClassAccess::EControl::ReferenceOrNone;

				return DcOk();
			}
			//	inline object read as struct, ie expanded
			else if (Type == EType::PropertyInstanced
				|| Type == EType::Root)
			{
				check(ClassObject);
				UClass* Cls = ClassObject->GetClass();

				Access.Name = Cls->GetFName();
				Access.Control = FDcClassAccess::EControl::ExpandObject;

				Property = Parent->Config.FirstProcessProperty(Cls->PropertyLink);
				if (Property == nullptr)
				{
					State = EState::ExpectEnd;
				}
				else
				{
					State = EState::ExpectKey;
				}
				return DcOk();
			}
			else
			{
				return DcNoEntry();
			}
		}
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectRoot << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateClass::ReadClassEndAccess(FDcPropertyReader* Parent, FDcClassAccess& Access)
{
	if (State == EState::ExpectEnd)
	{
		State = EState::Ended;

		Access.Name = Class->GetFName();
		Access.Control = Type == EType::Root
			? FDcClassAccess::EControl::ExpandObject
			: FDcClassAccess::EControl::ReferenceOrNone;

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectEnd << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateClass::ReadNone(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectNone)
	{
		State = EState::ExpectEnd;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectNone << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateClass::ReadObjectReference(FDcPropertyReader* Parent, UObject** OutPtr)
{
	if (State == EState::ExpectReference)
	{
		ReadOut(OutPtr, ClassObject);

		State = EState::ExpectEnd;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectReference << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateClass::SkipRead(FDcPropertyReader* Parent)
{
	if (State != EState::ExpectValue)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
		<< EState::ExpectValue << State
		<< Parent->FormatHighlight();

	EndValueRead(Parent);
	return DcOk();
}

void FDcReadStateClass::EndValueRead(FDcPropertyReader* Parent)
{
	check(State == EState::ExpectValue);
	Property = Parent->Config.NextProcessProperty(Property);
	if (Property == nullptr)
		State = EState::ExpectEnd;
	else
		State = EState::ExpectKey;
}

FDcResult FDcReadStateClass::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty)
{
	if (State == EState::ExpectRoot)
	{
		return ReadOutOk(OutProperty, Class);
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutProperty, Property);
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< State << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateClass::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr)
{
	if (State == EState::ExpectRoot)
	{
		return ReadOutOk(OutDataPtr, &ClassObject);
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutDataPtr, Property->ContainerPtrToValuePtr<void>(ClassObject));
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< State << Parent->FormatHighlight();
	}
}

EDcPropertyReadType FDcReadStateStruct::GetType()
{
	return EDcPropertyReadType::StructProperty;
}

FDcResult FDcReadStateStruct::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	if (State == EState::ExpectRoot)
	{
		ReadOut(OutPtr, EDcDataEntry::StructRoot);
		return DcOk();
	}
	else if (State == EState::ExpectEnd)
	{
		ReadOut(OutPtr, EDcDataEntry::StructEnd);
		return DcOk();
	}
	else if (State == EState::ExpectKey)
	{
		ReadOut(OutPtr, EDcDataEntry::Name);
		return DcOk();
	}
	else if (State == EState::ExpectValue)
	{
		check(Property);
		if (DcPropertyUtils::IsScalarArray(Property))
			return ReadOutOk(OutPtr, EDcDataEntry::ArrayRoot);
		else
			return ReadOutOk(OutPtr, DcPropertyUtils::PropertyToDataEntry(Property));
	}
	else if (State == EState::Ended)
	{
		ReadOut(OutPtr, EDcDataEntry::Ended);
		return DcOk();
	}
	else
	{
		return DcNoEntry();
	}
}

FDcResult FDcReadStateStruct::ReadName(FDcPropertyReader* Parent, FName* OutNamePtr)
{
	if (State == EState::ExpectKey)
	{
		check(Property);
		ReadOut(OutNamePtr, Property->GetFName());

		State = EState::ExpectValue;
		return DcOk();
	}
	else if (State == EState::ExpectValue)
	{
		FDcPropertyDatum Datum;
		DC_TRY(ReadDataEntry(Parent, FNameProperty::StaticClass(), Datum));

		ReadOut(OutNamePtr, Datum.CastFieldChecked<FNameProperty>()->GetPropertyValue(Datum.DataPtr));
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< (int)State << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateStruct::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass* ExpectedPropertyClass, FDcPropertyDatum& OutDatum)
{
	if (State == EState::ExpectKey)
	{
		// should've be already handled in `ReadName`
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectValue << (int)State
			<< Parent->FormatHighlight();
	}
	else if (State == EState::ExpectValue)
	{
		check(ExpectedPropertyClass);
		check(Property);

		DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, Property, ExpectedPropertyClass));

		OutDatum.Property = Property;
		OutDatum.DataPtr = Property->ContainerPtrToValuePtr<void>(StructPtr);

		EndValueRead(Parent);
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectValue << (int)State
			<< Parent->FormatHighlight();
	}
}

void FDcReadStateStruct::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	DcPropertyHighlight::FormatStruct(OutSegments, SegType, StructName, StructClass, Property);
}

FDcResult FDcReadStateStruct::ReadStructRootAccess(FDcPropertyReader* Parent, FDcStructAccess& Access)
{
	DC_TRY(DcPropertyReadStateDetails::HeuristicCheckStatesTooDeep(Parent));
	if (State == EState::ExpectRoot)
	{
		Access.Name = StructClass->GetFName();

		Property = Parent->Config.FirstProcessProperty(StructClass->PropertyLink);
		if (Property == nullptr)
		{
			State = EState::ExpectEnd;
		}
		else
		{
			State = EState::ExpectKey;
		}

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectRoot << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateStruct::ReadStructEndAccess(FDcPropertyReader* Parent, FDcStructAccess& Access)
{
	if (State == EState::ExpectEnd)
	{
		State = EState::Ended;

		Access.Name = StructClass->GetFName();

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectEnd << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateStruct::SkipRead(FDcPropertyReader* Parent)
{
	if (State != EState::ExpectValue)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
		<< EState::ExpectValue << State
		<< Parent->FormatHighlight();

	EndValueRead(Parent);
	return DcOk();
}

void FDcReadStateStruct::EndValueRead(FDcPropertyReader* Parent)
{
	check(State == EState::ExpectValue)
	Property = Parent->Config.NextProcessProperty(Property);
	if (Property == nullptr)
		State = EState::ExpectEnd;
	else
		State = EState::ExpectKey;
}

FDcResult FDcReadStateStruct::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty)
{
	if (State == EState::ExpectRoot)
	{
		return ReadOutOk(OutProperty, StructClass);
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutProperty, Property);
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< State << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateStruct::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr)
{
	if (State == EState::ExpectRoot)
	{
		return ReadOutOk(OutDataPtr, StructPtr);
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutDataPtr, Property->ContainerPtrToValuePtr<void>(StructPtr));
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< State << Parent->FormatHighlight();
	}
}


EDcPropertyReadType FDcReadStateMap::GetType()
{
	return EDcPropertyReadType::MapProperty;
}

FDcResult FDcReadStateMap::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	if (State == EState::ExpectRoot)
	{
		ReadOut(OutPtr, EDcDataEntry::MapRoot);
		return DcOk();
	}
	else if (State == EState::ExpectEnd)
	{
		ReadOut(OutPtr, EDcDataEntry::MapEnd);
		return DcOk();
	}
	else if (State == EState::ExpectKey)
	{
		ReadOut(OutPtr, DcPropertyUtils::PropertyToDataEntry(MapHelper.KeyProp));
		return DcOk();
	}
	else if (State == EState::ExpectValue)
	{
		ReadOut(OutPtr, DcPropertyUtils::PropertyToDataEntry(MapHelper.ValueProp));
		return DcOk();
	}
	else
	{
		return DcNoEntry();
	}
}

FDcResult FDcReadStateMap::ReadName(FDcPropertyReader* Parent, FName* OutNamePtr)
{
	FDcPropertyDatum Datum;
	DC_TRY(ReadDataEntry(Parent, FNameProperty::StaticClass(), Datum));

	ReadOut(OutNamePtr, Datum.CastFieldChecked<FNameProperty>()->GetPropertyValue(Datum.DataPtr));
	return DcOk();
}

FDcResult FDcReadStateMap::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass* ExpectedPropertyClass, FDcPropertyDatum& OutDatum)
{
	if (State == EState::Ended
		|| State == EState::ExpectRoot
		|| State == EState::ExpectEnd)
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< (int)State << Parent->FormatHighlight();

	if (State == EState::ExpectKey)
	{
		FProperty* KeyProperty = MapHelper.GetKeyProperty();
		DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, KeyProperty, ExpectedPropertyClass));

		while (SparseIndex < MapHelper.GetMaxIndex())
		{
			if (MapHelper.IsValidIndex(SparseIndex))
				break;
			SparseIndex += 1;
		}

		OutDatum.Property = KeyProperty;
		OutDatum.DataPtr = MapHelper.GetKeyPtr(SparseIndex);

		State = EState::ExpectValue;
		return DcOk();
	}
	else if (State == EState::ExpectValue)
	{
		FProperty* ValueProperty = MapHelper.GetValueProperty();
		DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, ValueProperty, ExpectedPropertyClass));

		OutDatum.Property = ValueProperty;
		OutDatum.DataPtr = MapHelper.GetValuePtr(SparseIndex);

		SparseIndex += 1;
		Index += 1;
		if (Index < MapHelper.Num())
			State = EState::ExpectKey;
		else
			State = EState::ExpectEnd;

		return DcOk();
	}
	else
	{
		return DcNoEntry();
	}
}

void FDcReadStateMap::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	DcPropertyHighlight::FormatMap(OutSegments, SegType, MapName, MapHelper.KeyProp, MapHelper.ValueProp, Index,
		State == EState::ExpectKey || State == EState::ExpectValue);
}

FDcResult FDcReadStateMap::ReadMapRoot(FDcPropertyReader* Parent)
{
	DC_TRY(DcPropertyReadStateDetails::HeuristicCheckStatesTooDeep(Parent));
	if (State == EState::ExpectRoot)
	{
		//	check map effectiveness
		if (!Parent->Config.ShouldProcessProperty(MapHelper.KeyProp)
			|| !Parent->Config.ShouldProcessProperty(MapHelper.ValueProp))
		{
			State = EState::ExpectEnd;
			return DcOk();
		}

		if (MapHelper.Num() == 0)
		{
			State = EState::ExpectEnd;
		}
		else
		{
			State = EState::ExpectKey;
		}
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectRoot << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateMap::ReadMapEnd(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectEnd)
	{
		State = EState::Ended;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectEnd << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateMap::SkipRead(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectKey)
	{
		State = EState::ExpectValue;
		return DcOk();
	}
	else if (State == EState::ExpectValue)
	{
		Index += 1;
		if (Index < MapHelper.Num())
			State = EState::ExpectKey;
		else
			State = EState::ExpectEnd;

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect2)
			<< EState::ExpectKey << EState::ExpectValue << State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateMap::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty)
{
	if (State == EState::ExpectRoot)
	{
		if (MapProperty == nullptr)
			return DC_FAIL(DcDReadWrite, PeekNoContainerProperty) << TEXT("Map");
		return ReadOutOk(OutProperty, MapProperty);
	}
	else if (State == EState::ExpectKey)
	{
		return ReadOutOk(OutProperty, MapHelper.KeyProp);
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutProperty, MapHelper.ValueProp);
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect2)
			<< EState::ExpectKey << EState::ExpectValue << State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateMap::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr)
{
	if (State == EState::ExpectKey)
	{
		return ReadOutOk(OutDataPtr, MapHelper.GetKeyPtr(Index));
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutDataPtr, MapHelper.GetValuePtr(Index));
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect2)
			<< EState::ExpectKey << EState::ExpectValue << State
			<< Parent->FormatHighlight();
	}
}

EDcPropertyReadType FDcReadStateArray::GetType()
{
	return EDcPropertyReadType::ArrayProperty;
}

FDcResult FDcReadStateArray::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	if (State == EState::ExpectRoot)
	{
		ReadOut(OutPtr, EDcDataEntry::ArrayRoot);
		return DcOk();
	}
	else if (State == EState::ExpectEnd)
	{
		ReadOut(OutPtr, EDcDataEntry::ArrayEnd);
		return DcOk();
	}
	else if (State == EState::ExpectItem)
	{
		auto& ArrayAccess = (DcSerDeCommon::FScriptArrayHelperAccess&)ArrayHelper;
		check(ArrayAccess.InnerProperty);
		ReadOut(OutPtr, DcPropertyUtils::PropertyToDataEntry(ArrayAccess.InnerProperty));
		return DcOk();
	}
	else if (State == EState::Ended)
	{
		ReadOut(OutPtr, EDcDataEntry::Ended);
		return DcOk();
	}
	else
	{
		return DcNoEntry();
	}
}

FDcResult FDcReadStateArray::ReadName(FDcPropertyReader* Parent, FName* OutNamePtr)
{
	FDcPropertyDatum Datum;
	DC_TRY(ReadDataEntry(Parent, FNameProperty::StaticClass(), Datum));

	ReadOut(OutNamePtr, Datum.CastFieldChecked<FNameProperty>()->GetPropertyValue(Datum.DataPtr));
	return DcOk();
}

FDcResult FDcReadStateArray::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass* ExpectedPropertyClass, FDcPropertyDatum& OutDatum)
{
	if (State == EState::Ended
		|| State == EState::ExpectRoot
		|| State == EState::ExpectEnd)
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< (int)State << Parent->FormatHighlight();

	check(State == EState::ExpectItem);
	auto& ArrayAccess = (DcSerDeCommon::FScriptArrayHelperAccess&)ArrayHelper;

	DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, ArrayAccess.InnerProperty, ExpectedPropertyClass));

	OutDatum.Property = ArrayAccess.InnerProperty;
	OutDatum.DataPtr = ArrayHelper.GetRawPtr(Index);

	Index += 1;
	if (Index < ArrayHelper.Num())
		State = EState::ExpectItem;
	else
		State = EState::ExpectEnd;

	return DcOk();
}

void FDcReadStateArray::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	auto& ArrayAccess = (DcSerDeCommon::FScriptArrayHelperAccess&)ArrayHelper;
	DcPropertyHighlight::FormatArray(OutSegments, SegType, ArrayName, ArrayAccess.InnerProperty, Index,
		State == EState::ExpectItem);
}

FDcResult FDcReadStateArray::ReadArrayRoot(FDcPropertyReader* Parent)
{
	DC_TRY(DcPropertyReadStateDetails::HeuristicCheckStatesTooDeep(Parent));
	auto& ArrayAccess = (DcSerDeCommon::FScriptArrayHelperAccess&)ArrayHelper;
	if (State == EState::ExpectRoot)
	{
		if (!Parent->Config.ShouldProcessProperty(ArrayAccess.InnerProperty))
		{
			State = EState::ExpectEnd;
			return DcOk();
		}

		if (ArrayHelper.Num() == 0)
		{
			State = EState::ExpectEnd;
		}
		else
		{
			State = EState::ExpectItem;
		}
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectRoot << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateArray::ReadArrayEnd(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectEnd)
	{
		State = EState::Ended;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectEnd << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateArray::SkipRead(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectItem)
	{
		Index += 1;
		if (Index < ArrayHelper.Num())
			State = EState::ExpectItem;
		else
			State = EState::ExpectEnd;

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< EState::ExpectItem << State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateArray::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty)
{
	if (State == EState::ExpectRoot)
	{
		if (ArrayProperty == nullptr)
			return DC_FAIL(DcDReadWrite, PeekNoContainerProperty) << TEXT("Array");
		return ReadOutOk(OutProperty, ArrayProperty);
	}
	else if (State == EState::ExpectItem)
	{
		auto& ArrayAccess = (DcSerDeCommon::FScriptArrayHelperAccess&)ArrayHelper;
		return ReadOutOk(OutProperty, ArrayAccess.InnerProperty);
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< State << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateArray::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr)
{
	if (State != EState::ExpectItem)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
		<< EState::ExpectItem << State
		<< Parent->FormatHighlight();

	return ReadOutOk(OutDataPtr, ArrayHelper.GetRawPtr(Index));
}

EDcPropertyReadType FDcReadStateSet::GetType()
{
	return EDcPropertyReadType::SetProperty;
}

FDcResult FDcReadStateSet::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	if (State == EState::ExpectRoot)
	{
		ReadOut(OutPtr, EDcDataEntry::SetRoot);
		return DcOk();
	}
	else if (State == EState::ExpectEnd)
	{
		ReadOut(OutPtr, EDcDataEntry::SetEnd);
		return DcOk();
	}
	else if (State == EState::ExpectItem)
	{
		ReadOut(OutPtr, DcPropertyUtils::PropertyToDataEntry(SetHelper.ElementProp));
		return DcOk();
	}
	else if (State == EState::Ended)
	{
		ReadOut(OutPtr, EDcDataEntry::Ended);
		return DcOk();
	}
	else
	{
		return DcNoEntry();
	}
}

FDcResult FDcReadStateSet::ReadName(FDcPropertyReader* Parent, FName* OutNamePtr)
{
	FDcPropertyDatum Datum;
	DC_TRY(ReadDataEntry(Parent, FNameProperty::StaticClass(), Datum));

	ReadOut(OutNamePtr, Datum.CastFieldChecked<FNameProperty>()->GetPropertyValue(Datum.DataPtr));

	return DcOk();
}

FDcResult FDcReadStateSet::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass* ExpectedPropertyClass, FDcPropertyDatum& OutDatum)
{
	if (State != EState::ExpectItem)
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< (int)State << Parent->FormatHighlight();

	check(State == EState::ExpectItem);

	DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, SetHelper.ElementProp, ExpectedPropertyClass));

	while (SparseIndex < SetHelper.GetMaxIndex())
	{
		if (SetHelper.IsValidIndex(SparseIndex))
			break;
		SparseIndex += 1;
	}

	OutDatum.Property = SetHelper.ElementProp;
	OutDatum.DataPtr = SetHelper.GetElementPtr(SparseIndex);

	SparseIndex += 1;
	Index += 1;
	if (Index < SetHelper.Num())
		State = EState::ExpectItem;
	else
		State = EState::ExpectEnd;

	return DcOk();
}

void FDcReadStateSet::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	DcPropertyHighlight::FormatSet(OutSegments, SegType, SetName, SetHelper.ElementProp, Index,
		State == EState::ExpectItem);
}

FDcResult FDcReadStateSet::SkipRead(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectItem)
	{
		Index += 1;
		if (Index < SetHelper.Num())
			State = EState::ExpectItem;
		else
			State = EState::ExpectEnd;

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< EState::ExpectItem << State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateSet::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty)
{
	if (State == EState::ExpectRoot)
	{
		if (SetProperty == nullptr)
			return DC_FAIL(DcDReadWrite, PeekNoContainerProperty) << TEXT("Set");
		return ReadOutOk(OutProperty, SetProperty);
	}
	else if (State == EState::ExpectItem)
	{
		return ReadOutOk(OutProperty, SetHelper.ElementProp);
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< EState::ExpectItem << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateSet::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr)
{
	if (State != EState::ExpectItem)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
		<< EState::ExpectItem << State
		<< Parent->FormatHighlight();

	return ReadOutOk(OutDataPtr, SetHelper.GetElementPtr(Index));
}

FDcResult FDcReadStateSet::ReadSetRoot(FDcPropertyReader* Parent)
{
	DC_TRY(DcPropertyReadStateDetails::HeuristicCheckStatesTooDeep(Parent));
	if (State == EState::ExpectRoot)
	{
		if (!Parent->Config.ShouldProcessProperty(SetHelper.ElementProp))
		{
			State = EState::ExpectEnd;
			return DcOk();
		}

		State = SetHelper.Num() == 0 ? EState::ExpectEnd : EState::ExpectItem;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectRoot << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateSet::ReadSetEnd(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectEnd)
	{
		State = EState::Ended;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectEnd << (int)State
			<< Parent->FormatHighlight();
	}
}

#if !UE_VERSION_OLDER_THAN(5, 4, 0)
EDcPropertyReadType FDcReadStateOptional::GetType()
{
	return EDcPropertyReadType::OptionalProperty;
}

FDcResult FDcReadStateOptional::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	if (State == EState::ExpectRoot)
	{
		return ReadOutOk(OutPtr, EDcDataEntry::OptionalRoot);
	}
	else if (State == EState::ExpectEnd)
	{
		return ReadOutOk(OutPtr, EDcDataEntry::OptionalEnd);
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutPtr, DcPropertyUtils::PropertyToDataEntry(OptionalProperty->GetValueProperty()));
	}
	else if (State == EState::ExpectNone)
	{
		return ReadOutOk(OutPtr, EDcDataEntry::None);
	}
	else if (State == EState::Ended)
	{
		return ReadOutOk(OutPtr, EDcDataEntry::Ended);
	}
	else
	{
		return DcNoEntry();
	}
}

FDcResult FDcReadStateOptional::ReadName(FDcPropertyReader* Parent, FName* OutNamePtr)
{
	if (State == EState::ExpectValue)
	{
		FDcPropertyDatum Datum;
		DC_TRY(ReadDataEntry(Parent, FNameProperty::StaticClass(), Datum));
		return ReadOutOk(OutNamePtr, Datum.CastFieldChecked<FNameProperty>()->GetPropertyValue(Datum.DataPtr));
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< (int)State << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateOptional::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass* ExpectedPropertyClass, FDcPropertyDatum& OutDatum)
{
	if (State != EState::ExpectValue)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectValue << (int)State
			<< Parent->FormatHighlight();

	check(ExpectedPropertyClass);
	FProperty* ValueProperty = OptionalProperty->GetValueProperty();

	DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, ValueProperty, ExpectedPropertyClass));

	OutDatum.Property = ValueProperty;
	OutDatum.DataPtr = OptionalProperty->GetValuePointerForReadOrReplace(OptionalPtr);

	State = EState::ExpectEnd;

	return DcOk();
}

FDcResult FDcReadStateOptional::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty)
{
	if (State == EState::ExpectRoot)
	{
		return ReadOutOk(OutProperty, OptionalProperty);
	}
	else if (State == EState::ExpectValue)
	{
		return ReadOutOk(OutProperty, OptionalProperty->GetValueProperty());
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateNoExpect)
			<< State << Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateOptional::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr)
{
	if (State != EState::ExpectValue)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectValue << (int)State
			<< Parent->FormatHighlight();

	return ReadOutOk(OutDataPtr, OptionalProperty->GetValuePointerForReadOrReplace(OptionalPtr));
}

FDcResult FDcReadStateOptional::SkipRead(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectValue)
	{
		State = EState::ExpectEnd;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectValue << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateOptional::ReadOptionalRoot(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectRoot)
	{
		if (OptionalProperty->IsSet(OptionalPtr))
		{
			State = EState::ExpectValue;
		}
		else
		{
			State = EState::ExpectNone;
		}

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectRoot << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateOptional::ReadNone(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectNone)
	{
		State = EState::ExpectEnd;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectNone << (int)State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateOptional::ReadOptionalEnd(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectEnd)
	{
		State = EState::Ended;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< (int)EState::ExpectEnd << (int)State
			<< Parent->FormatHighlight();
	}
}

void FDcReadStateOptional::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	DcPropertyHighlight::FormatOptional(OutSegments, SegType, OptionalProperty->GetFName(), OptionalProperty->GetValueProperty());
}
#endif // !UE_VERSION_OLDER_THAN(5, 4, 0)

EDcPropertyReadType FDcReadStateScalar::GetType()
{
	return EDcPropertyReadType::ScalarProperty;
}

FDcResult FDcReadStateScalar::PeekRead(FDcPropertyReader* Parent, EDcDataEntry* OutPtr)
{
	if (State == EState::ExpectScalar
		|| State == EState::ExpectArrayItem)
	{
		return ReadOutOk(OutPtr, DcPropertyUtils::PropertyToDataEntry(ScalarField));
	}
	else if (State == EState::ExpectArrayRoot)
	{
		return ReadOutOk(OutPtr, EDcDataEntry::ArrayRoot);
	}
	else if (State == EState::ExpectArrayEnd)
	{
		return ReadOutOk(OutPtr, EDcDataEntry::ArrayEnd);
	}
	else
	{
		return ReadOutOk(OutPtr, EDcDataEntry::Ended);
	}
}

FDcResult FDcReadStateScalar::ReadName(FDcPropertyReader* Parent, FName* OutNamePtr)
{
	FDcPropertyDatum Datum;
	DC_TRY(ReadDataEntry(Parent, FNameProperty::StaticClass(), Datum));
	ReadOut(OutNamePtr, Datum.CastFieldChecked<FNameProperty>()->GetPropertyValue(Datum.DataPtr));
	return DcOk();
}

FDcResult FDcReadStateScalar::ReadDataEntry(FDcPropertyReader* Parent, FFieldClass* ExpectedPropertyClass, FDcPropertyDatum& OutDatum)
{
	if (State != EState::ExpectScalar
		&& State != EState::ExpectArrayItem)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect2)
			<< EState::ExpectScalar << EState::ExpectArrayItem << State
			<< Parent->FormatHighlight();

	DC_TRY(DcPropertyReadStateDetails::CheckExpectedProperty(Parent, ScalarField, ExpectedPropertyClass));

	if (State == EState::ExpectScalar)
	{
		OutDatum.Property = ScalarField;
		OutDatum.DataPtr = ScalarPtr;
		State = EState::Ended;
	}
	else
	{
		OutDatum.Property = ScalarField;
		OutDatum.DataPtr = (uint8*)ScalarPtr + (PTRINT)(DcPropertyUtils::ElementSize(ScalarField) * Index);

		++Index;
		if (Index == ScalarField->ArrayDim)
			State = EState::ExpectArrayEnd;
	}

	return DcOk();
}

FDcResult FDcReadStateScalar::SkipRead(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectScalar)
	{
		State = EState::Ended;
		return DcOk();
	}
	else if (State == EState::ExpectArrayItem)
	{
		++Index;
		if (Index == ScalarField->ArrayDim)
			State = EState::ExpectArrayEnd;

		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect2)
			<< EState::ExpectScalar << EState::ExpectArrayItem << State
			<< Parent->FormatHighlight();
	}
}

FDcResult FDcReadStateScalar::PeekReadProperty(FDcPropertyReader* Parent, FFieldVariant* OutProperty)
{
	if (State != EState::ExpectScalar
		&& State != EState::ExpectArrayItem)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect2)
			<< EState::ExpectScalar << EState::ExpectArrayItem << State
			<< Parent->FormatHighlight();

	return ReadOutOk(OutProperty, ScalarField);
}

FDcResult FDcReadStateScalar::PeekReadDataPtr(FDcPropertyReader* Parent, void** OutDataPtr)
{
	if (State != EState::ExpectScalar
		&& State != EState::ExpectArrayItem)
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect2)
			<< EState::ExpectScalar << EState::ExpectArrayItem << State
			<< Parent->FormatHighlight();

	return ReadOutOk(OutDataPtr, (uint8*)ScalarPtr + (PTRINT)(DcPropertyUtils::ElementSize(ScalarField) * Index));
}

FDcResult FDcReadStateScalar::ReadArrayRoot(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectArrayRoot)
	{
		//	ArrayDim > 1 so there's at least some item to read
		State = EState::ExpectArrayItem;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< EState::ExpectArrayRoot << State
			<< Parent->FormatHighlight();
	}

}

FDcResult FDcReadStateScalar::ReadArrayEnd(FDcPropertyReader* Parent)
{
	if (State == EState::ExpectArrayEnd)
	{
		State = EState::Ended;
		return DcOk();
	}
	else
	{
		return DC_FAIL(DcDReadWrite, InvalidStateWithExpect)
			<< EState::ExpectArrayEnd << State
			<< Parent->FormatHighlight();
	}
}


void FDcReadStateScalar::FormatHighlightSegment(TArray<FString>& OutSegments, DcPropertyHighlight::EFormatSeg SegType)
{
	DcPropertyHighlight::FormatScalar(OutSegments, SegType, ScalarField, Index, State == EState::ExpectArrayItem);
}
