#include "DcTestProperty5.h"

#include "DataConfig/Automation/DcAutomation.h"
#include "DataConfig/Automation/DcAutomationUtils.h"
#include "DataConfig/Extra/Misc/DcTestCommon.h"
#include "DataConfig/Json/DcJsonReader.h"
#include "DataConfig/Diagnostic/DcDiagnosticReadWrite.h"

#include "Internationalization/StringTableRegistry.h"

void FDcTestStructWithText::MakeFixture()
{
	// text test fixtures
	if (!FStringTableRegistry::Get().FindStringTable("/Bogus/Path/To/Table"))
	{
		LOCTABLE_NEW("/Bogus/Path/To/Table", "DataConfig.Tests");
		LOCTABLE_SETSTRING("/Bogus/Path/To/Table", "FOO_TEST", "Localize String Foo");
	}

	TextFieldAlpha = NSLOCTEXT("DataConfig.Tests", "BAR_TEST", "Localize String Bar");
	TextFieldBeta = FText::FromStringTable(FName(TEXT("/Bogus/Path/To/Table")), TEXT("FOO_TEST"));
}

DC_TEST("DataConfig.Core.Property.Defaults")
{
	FString Str = TEXT(R"(
		{
			"StrFieldWithDefault" : "Bar",
			"IntFieldWithDefault" : 123,

			"StrArrFieldWithDefault" : ["One", "Two", "Three", "Four"],
			"StrSetFieldWithDefault" : ["Foo", "Bar"],
			"StringIntMapFieldWithDefault" : {
				"Five" : 5,
				"Ten" : 10
			}
		}
	)");

	FDcJsonReader Reader(Str);
	FDcTestStructWithDefaults Dest;

	UTEST_OK("Deserialize With Defaults", DcAutomationUtils::DeserializeFrom(&Reader, FDcPropertyDatum(&Dest)));

	FDcTestStructWithDefaults Expect;
	Expect.StrFieldWithDefault = "Bar";
	Expect.IntFieldWithDefault = 123;

	Expect.StrArrFieldWithDefault = {"One", "Two", "Three", "Four"};
	Expect.StrSetFieldWithDefault = {"Foo", "Bar"};
	Expect.StringIntMapFieldWithDefault = {
		{"Five", 5},
		{"Ten", 10},
	};

	UTEST_OK("Deserialize With Defaults", DcAutomationUtils::TestReadDatumEqual(FDcPropertyDatum(&Dest), FDcPropertyDatum(&Expect)));

	return true;
}

DC_TEST("DataConfig.Core.Property.Text")
{
	FString Str = TEXT(R"-(

		{
			"TextFieldAlpha" : "NSLOCTEXT(\"DataConfig.Tests\", \"BAR_TEST\", \"Localize String Bar\")",
			"TextFieldBeta" : "LOCTABLE(\"/Bogus/Path/To/Table\", \"FOO_TEST\")"
		}

	)-");


	FDcJsonReader Reader(Str);
	FDcTestStructWithText Dest;

	FDcTestStructWithText Expect;
	Expect.MakeFixture();

	UTEST_OK("Deserialize Text", DcAutomationUtils::DeserializeFrom(&Reader, FDcPropertyDatum(&Dest)));
	UTEST_OK("Deserialize Text", DcAutomationUtils::TestReadDatumEqual(FDcPropertyDatum(&Dest), FDcPropertyDatum(&Expect)));

	return true;
}

DC_TEST("DataConfig.Core.Property.TextEqualCheck")
{
	FDcTestStructWithText Foo;
	FDcTestStructWithText Bar;

	UTEST_OK("Text Equal Check", DcAutomationUtils::TestReadDatumEqual(FDcPropertyDatum(&Foo), FDcPropertyDatum(&Bar)));

	Foo.TextFieldAlpha = NSLOCTEXT("DataConfig.Tests", "FOO_ALT", "Foo Alt");
	Bar.TextFieldAlpha = NSLOCTEXT("DataConfig.Tests", "BAR_ALT", "Foo Alt");

	UTEST_DIAG("Text Equal Check", DcAutomationUtils::TestReadDatumEqual(FDcPropertyDatum(&Foo), FDcPropertyDatum(&Bar)),
		DcDReadWrite, DataTypeUnequalLhsRhs);

	Bar.TextFieldAlpha = NSLOCTEXT("DataConfig.Tests.Alt", "FOO_ALT", "Foo Alt");

	UTEST_DIAG("Text Equal Check", DcAutomationUtils::TestReadDatumEqual(FDcPropertyDatum(&Foo), FDcPropertyDatum(&Bar)),
		DcDReadWrite, DataTypeUnequalLhsRhs);

	Bar.TextFieldAlpha = FText();

	UTEST_DIAG("Text Equal Check", DcAutomationUtils::TestReadDatumEqual(FDcPropertyDatum(&Foo), FDcPropertyDatum(&Bar)),
		DcDReadWrite, DataTypeUnequalLhsRhs);

	Bar.TextFieldAlpha = NSLOCTEXT("DataConfig.Tests", "FOO_ALT", "Foo Alt");

	UTEST_OK("Text Equal Check", DcAutomationUtils::TestReadDatumEqual(FDcPropertyDatum(&Foo), FDcPropertyDatum(&Bar)));

	return true;
}
