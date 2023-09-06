
#include "orbi.h"
#include <iostream>
#include <fstream>
#include <optional>

#include <UObject/UObjectBase.h>
#include <UObject/UObjectIterator.h>
#include <UObject/PropertyIterator.h>
#include "UObject/Package.h"
#include <EngineUtils.h>
#include "Misc/Paths.h"
#include "InputAction.h"
#include "InputActionValue.h"
#if WITH_EDITOR
#include <LevelEditor.h>
#endif

#ifdef _MSC_VER
# define M_PI 3.141592653
#endif
#include <libport/cmath>
#undef check
#undef PI

#define STATIC_BUILD 1
#define URBI_NO_RTTI
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include <functional>
#ifndef _WIN32
namespace std
{
  template<typename a, typename b> using unary_function = __unary_function<a, b>;
}
#endif
#include <urbi/lowlevel.hh>
#include <libport/time.hh>
#include <libport/statistics.hh>
#include <urbi/uobject.hh>
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
//#include "Kismet2/KismetReinstanceUtilities.h"

#define LOCTEXT_NAMESPACE "FurbiModule"

COREUOBJECT_API UClass* Z_Construct_UClass_UObject_NoRegister();
using Destructors = std::vector <std::function<void(std::vector<unsigned char>&)>>;

inline std::string str(FString const& fs)
{
  return std::string(TCHAR_TO_UTF8(*fs));
}
struct ExtraOut
{
	void* data;
	int offset;
	UScriptStruct* type;
};
class Unreal: public urbi::UObject
{
  public:
    Unreal(std::string const& s);
    std::string findClass(std::string const &name);
    std::string instantiate(std::string const &classPtr);
    std::string holdPtr(std::string const &sPtr);
    std::string getActorComponentPtr(std::string const &actor, std::string const &component);
    urbi::UVar rootDir;
    double add(double va, double vb);
    std::vector<std::string> listActors();
    std::vector<std::string> listComponents(std::string const& actor);
    std::vector<unsigned char> marshall(UFunction *func, urbi::UList const &args, FProperty* ret, std::vector<ExtraOut>& extraOut, Destructors& destructors);
    urbi::UValue unmarshallClass(UClass *prop, void *data, int &sz);
    urbi::UValue unmarshall(FProperty *prop, void *data, int &sz);
    urbi::UValue unmarshallStruct(UStruct *ustruct, void *data, int &sz);
    urbi::UValue unmarshallFunc(UFunction *func, void *data, int &sz);
    std::vector<std::string> listFields(std::string const &actor, std::string const &component);
    std::vector<std::string> listProperties(std::string const &actor, std::string const &component);
    std::vector<std::string> listData(std::string const &actor, std::string const &component);
    std::string getFunctionSignature(std::string const &actor, std::string const &component, std::string const &function);
    urbi::UValue callFunction(std::string const& actor, std::string const& component, std::string const& function, urbi::UValue const& args);
    urbi::UValue callFunctionDump(std::string const& actor, std::string const& component, std::string const& function, urbi::UValue const& args);
    void setPropertyValue(std::string const &actor, std::string const &component, std::string const &propName, urbi::UValue const &val);
    urbi::UValue getPropertyValue(std::string const &actor, std::string const &component, std::string const &propName);
    void marshallArray(std::vector<unsigned char>& res, urbi::UList const& data, FProperty* innerProp, Destructors& destructors);
    void marshallOne(std::vector<unsigned char> &res, FProperty *prop, urbi::UValue const &v, std::vector<ExtraOut>& extraOut, int start, Destructors& destructors);
    std::string getFunctionReturnType(std::string const& objPtr, std::string const& funcName);
    std::string getPropertyType(std::string const& optr, std::string const& pname);
    void wipeCache();
	  void processPendingEvents();
   	void registerCallResult(int64 uid, urbi::UValue res);
   	void checkParseResult();
   	urbi::UValue parseResult;
   	std::string parseResultString;
  private:
    std::optional<AActor*> getActor(std::string const& name);
    std::optional<UActorComponent*> getActorComponent(std::string const& actor, std::string const& component);
    ::UObject* getUObject(std::string const& actor, std::string const& component);
    UFunction* getFunction(::UObject* uo, std::string const& function);
    FProperty *getProperty(::UObject *uo, std::string const& propName);
    std::vector<std::string> listEnhancedInputs(std::string const& objPtr);
    std::string bindEnhancedInput(std::string const& objPtr, std::string const& name, int what);
    std::string bindInput(std::string const& objPtr, std::string const& name, int what);
    std::string bindDelegate(std::string const& objPtr, std::string delegateName);

    std::string getClass(std::string const& objPtr);

    std::unordered_map<std::string, AActor *> actors;
    std::unordered_map<std::string, UActorComponent*> actorComponents;
    template<int I>
    void marshallArraySized(std::vector<unsigned char>& res, urbi::UList const& data, FProperty* innerProp, Destructors& destructors);
};
static Unreal* unrealInstance = nullptr;

struct ClassPropModel
{
	std::string name;
	urbi::UDataType type;
	urbi::UDataType innerType; // for lists
	bool isPtr = false;
	bool isInnerPtr = false;
	std::string structName;
	std::string innerStructName;
	int offset;
	int size = 0;
	bool valid = true;
	int propSize() const;
};
struct ClassFuncModel
{
  std::string name;
  std::vector<ClassPropModel> signature;
  int funcSize() const;
};
struct ClassModel
{
	std::string name;
	std::vector<ClassPropModel> props;
	std::vector<ClassFuncModel> funcs;
};

int ClassPropModel::propSize() const
{
  if (size)
    return size;
  if (isPtr || type == urbi::DATA_DOUBLE)
    return 8;
  else if (type == urbi::DATA_LIST || type == urbi::DATA_STRING)
    return 16;
  else
    return 0;
}
int ClassFuncModel::funcSize() const
{
  int res = 0;
  for (auto const& p: signature)
    res += p.propSize();
  return res;
}

// Globals for Unreal introspection of generated urbiscript components

static const int MAX_CLASS_COUNT = 10;
static ClassModel gClassModels[MAX_CLASS_COUNT];
static int gClassCount = 0;
static int gOldClassCount = 0;
static UClass* gInnerClassPointers[MAX_CLASS_COUNT];
static UClass* gOuterClassPointers[MAX_CLASS_COUNT];
static UFunction* gFunctions[MAX_CLASS_COUNT][50];
static UECodeGen_Private::FPropertyParamsBase* gFProps[MAX_CLASS_COUNT][50][30];
static int gGenerationCounter = 0;
static UScriptStruct* gKnownStructs[50];
static int gKnownStructsPosition = 0;

template<int I> UScriptStruct* KnownCtor()
{
  return gKnownStructs[I];
}
using SSFactory = UScriptStruct* (*)();

// Find an existing struct and generate a dummy construct method that returns it
SSFactory GenRecordKnownStructConstructor(std::string const& structName, ClassPropModel* target=nullptr)
{
  auto* hit = FindObject<UScriptStruct>(ANY_PACKAGE, *FString(structName.c_str()));
  if (!hit)
  {
    UE_LOG(LogScript, Error, TEXT("failed to locate struct: %s"),  *FString(structName.c_str()));
    return nullptr;
  }
  int pos = gKnownStructsPosition++;
  gKnownStructs[pos] = hit;
  if (target != nullptr)
    target->size = hit->GetStructureSize();
  if (pos == 0)
    return KnownCtor<0>;
  /*`for i in range(1, 50):
       print(f"  else if (pos == {i}) return KnownCtor<{i}>;")
  */
  else if (pos == 1) return KnownCtor<1>;
  else if (pos == 2) return KnownCtor<2>;
  else if (pos == 3) return KnownCtor<3>;
  else if (pos == 4) return KnownCtor<4>;
  else if (pos == 5) return KnownCtor<5>;
  else if (pos == 6) return KnownCtor<6>;
  else if (pos == 7) return KnownCtor<7>;
  else if (pos == 8) return KnownCtor<8>;
  else if (pos == 9) return KnownCtor<9>;
  else if (pos == 10) return KnownCtor<10>;
  else if (pos == 11) return KnownCtor<11>;
  else if (pos == 12) return KnownCtor<12>;
  else if (pos == 13) return KnownCtor<13>;
  else if (pos == 14) return KnownCtor<14>;
  else if (pos == 15) return KnownCtor<15>;
  else if (pos == 16) return KnownCtor<16>;
  else if (pos == 17) return KnownCtor<17>;
  else if (pos == 18) return KnownCtor<18>;
  else if (pos == 19) return KnownCtor<19>;
  else if (pos == 20) return KnownCtor<20>;
  else if (pos == 21) return KnownCtor<21>;
  else if (pos == 22) return KnownCtor<22>;
  else if (pos == 23) return KnownCtor<23>;
  else if (pos == 24) return KnownCtor<24>;
  else if (pos == 25) return KnownCtor<25>;
  else if (pos == 26) return KnownCtor<26>;
  else if (pos == 27) return KnownCtor<27>;
  else if (pos == 28) return KnownCtor<28>;
  else if (pos == 29) return KnownCtor<29>;
  else if (pos == 30) return KnownCtor<30>;
  else if (pos == 31) return KnownCtor<31>;
  else if (pos == 32) return KnownCtor<32>;
  else if (pos == 33) return KnownCtor<33>;
  else if (pos == 34) return KnownCtor<34>;
  else if (pos == 35) return KnownCtor<35>;
  else if (pos == 36) return KnownCtor<36>;
  else if (pos == 37) return KnownCtor<37>;
  else if (pos == 38) return KnownCtor<38>;
  else if (pos == 39) return KnownCtor<39>;
  else if (pos == 40) return KnownCtor<40>;
  else if (pos == 41) return KnownCtor<41>;
  else if (pos == 42) return KnownCtor<42>;
  else if (pos == 43) return KnownCtor<43>;
  else if (pos == 44) return KnownCtor<44>;
  else if (pos == 45) return KnownCtor<45>;
  else if (pos == 46) return KnownCtor<46>;
  else if (pos == 47) return KnownCtor<47>;
  else if (pos == 48) return KnownCtor<48>;
  else if (pos == 49) return KnownCtor<49>;

//END
  else
    return nullptr;
}

#if WITH_METADATA
const UECodeGen_Private::FMetaDataPairParam GenericPropMeta[] = {
	{ "Category", "UrbiComponent" },
	{ "ModuleRelativePath", "Public/orbi.h" },
};
#endif

// Generate introspection data for function arguments
std::vector<UECodeGen_Private::FPropertyParamsBase*>
makeUnrealProperties(std::vector<ClassPropModel>& props)
{
  EPropertyFlags pfarg = (EPropertyFlags)0x0010000000000080;
  EPropertyFlags pfret = (EPropertyFlags)0x0010000000000580;
	EPropertyFlags pfa = (EPropertyFlags)0x0000000000000000;
  std::vector<UECodeGen_Private::FPropertyParamsBase*> res;
  int coffset = 0;
  for (auto& p: props)
  {
    EPropertyFlags pf = (p.name == "ReturnValue") ? pfret : pfarg;
    auto type = p.type;
		auto isPtr = p.isPtr;
		UECodeGen_Private::EPropertyGenFlags utype = UECodeGen_Private::EPropertyGenFlags::Double;
		if (isPtr)
			utype = UECodeGen_Private::EPropertyGenFlags::Object;
		else if (type == urbi::DATA_DOUBLE)
			utype = UECodeGen_Private::EPropertyGenFlags::Double;
		else if (type == urbi::DATA_STRING)
			utype = UECodeGen_Private::EPropertyGenFlags::Str;
		else if (type == urbi::DATA_LIST)
			utype = UECodeGen_Private::EPropertyGenFlags::Array;
		else
		  continue;
		if (utype == UECodeGen_Private::EPropertyGenFlags::Array)
		{
			auto itype = p.innerType;
			auto iisPtr = p.isInnerPtr;
			UECodeGen_Private::EPropertyGenFlags iutype;
			if (iisPtr)
				iutype = UECodeGen_Private::EPropertyGenFlags::Object;
			else if (itype == urbi::DATA_DOUBLE)
				iutype = UECodeGen_Private::EPropertyGenFlags::Double;
			else if (itype == urbi::DATA_STRING)
				iutype = UECodeGen_Private::EPropertyGenFlags::Str;
			else continue;
			if (iisPtr)
				res.push_back((UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FObjectPropertyParams(UECodeGen_Private::FObjectPropertyParams
				{
				  p.name.c_str(),
					nullptr,
					pfa,
					iutype,
					RF_Public | RF_Transient | RF_MarkAsNative,
					1,
					nullptr, // GetSetter<I>(i),
					nullptr, //GetGetter<I>(i),
					0,
					Z_Construct_UClass_UObject_NoRegister,
					METADATA_PARAMS(GenericPropMeta, 2)
				}));
			else if (p.innerStructName.length())
			{
			  res.push_back((UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FStructPropertyParams(UECodeGen_Private::FStructPropertyParams
				{
					  p.name.c_str(),
						nullptr,
						pfa,
						UECodeGen_Private::EPropertyGenFlags::Struct,
						RF_Public | RF_Transient | RF_MarkAsNative,
						1,
						nullptr, //GetSetter<I>(i),
						nullptr, //GetGetter<I>(i),
						0,
						GenRecordKnownStructConstructor(p.innerStructName),
						METADATA_PARAMS(GenericPropMeta, 2)
				}));
			}
			else
				res.push_back((UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FGenericPropertyParams(UECodeGen_Private::FGenericPropertyParams
				{
					  p.name.c_str(),
						nullptr,
						pfa,
						iutype,
						RF_Public | RF_Transient | RF_MarkAsNative,
						1,
						nullptr, //GetSetter<I>(i),
						nullptr, //GetGetter<I>(i),
						0,
						METADATA_PARAMS(GenericPropMeta, 2)
				}));
			res.push_back( (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FArrayPropertyParams(UECodeGen_Private::FArrayPropertyParams
			{
				p.name.c_str(),
				nullptr,
				pf,
				utype,
				RF_Public | RF_Transient | RF_MarkAsNative,
				1,
				nullptr, //GetSetter<I>(i),
				nullptr, //GetGetter<I>(i),
				coffset,
				EArrayPropertyFlags::None,
				METADATA_PARAMS(GenericPropMeta, 2)
			}));
		}
		else
		{
			if (isPtr)
				res.push_back( (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FObjectPropertyParams(UECodeGen_Private::FObjectPropertyParams
				{
					p.name.c_str(),
					nullptr,
					pf,
					utype,
					RF_Public | RF_Transient | RF_MarkAsNative,
					1,
					nullptr, //GetSetter<I>(i),
					nullptr, //GetGetter<I>(i),
					coffset,
					Z_Construct_UClass_UObject_NoRegister,
					METADATA_PARAMS(GenericPropMeta, 2)
				}));
		  else if (p.structName.length())
			{
			  res.push_back((UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FStructPropertyParams(UECodeGen_Private::FStructPropertyParams
				{
					  p.name.c_str(),
						nullptr,
						pf,
						UECodeGen_Private::EPropertyGenFlags::Struct,
						RF_Public | RF_Transient | RF_MarkAsNative,
						1,
						nullptr, //GetSetter<I>(i),
						nullptr, //GetGetter<I>(i),
						coffset,
						GenRecordKnownStructConstructor(p.structName, &p),
						METADATA_PARAMS(GenericPropMeta, 2)
				}));
			}
			else
				res.push_back( (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FGenericPropertyParams(UECodeGen_Private::FGenericPropertyParams
				{
					p.name.c_str(),
					nullptr,
					pf,
					utype,
					RF_Public | RF_Transient | RF_MarkAsNative,
					1,
					nullptr, //GetSetter<I>(i),
					nullptr, //GetGetter<I>(i),
					coffset,
					METADATA_PARAMS(GenericPropMeta, 2)
				}));
		}
		coffset += p.propSize();
  }
  
  return res;
}

static void ConstructFields(UUrbiComponent* obj, int cls)
{
	obj->classIndex = cls;
	obj->className = FString(gClassModels[cls].name.c_str());
	memset(obj->buffer, 0, sizeof(obj->buffer)); // FIXME how is that for default construction? :)
}

// so you want a different vtable eh? Note: attempt at hot reload but so far we dropped it
class Twin: public UUrbiComponent
{
public:
  using UUrbiComponent::UUrbiComponent;
  virtual void ProcessEvent( UFunction* Function, void* Parms ) override
  {
    UUrbiComponent::ProcessEvent(Function, Parms);
  }
};

template<int I> void InternalConstructorHelper(const FObjectInitializer& X)
{
	UUrbiComponent* res = new((EInternal*)X.GetObj())UUrbiComponent(X);
	ConstructFields(res, I);
}
template<int I> UObject* InternalVTableHelper(FVTableHelper& Helper)
{
	UUrbiComponent* res;
	if (gGenerationCounter%2)
	  res = new (EC_InternalUseOnlyConstructor, (UObject*)GetTransientPackage(), NAME_None, RF_NeedLoad | RF_ClassDefaultObject | RF_TagGarbageTemp) UUrbiComponent(Helper);
	else
	  res = new (EC_InternalUseOnlyConstructor, (UObject*)GetTransientPackage(), NAME_None, RF_NeedLoad | RF_ClassDefaultObject | RF_TagGarbageTemp) Twin(Helper);
	ConstructFields(res, I);
	return res;
}

template<int I> UClass* ConstructInner();

// register native function pointers on a UClass
template<int I> void RegisterNatives()
{
  // Note: Doesn't that need to be static?
  UClass* Class = ConstructInner<I>();
  FNameNativePtrPair Funcs[] = {
  /*`for i in range(50):
        print(f"    {{\"function\", &UUrbiComponent::execfunction<I,{i}>}},")
   */
    {"function", &UUrbiComponent::execfunction<I,0>},
    {"function", &UUrbiComponent::execfunction<I,1>},
    {"function", &UUrbiComponent::execfunction<I,2>},
    {"function", &UUrbiComponent::execfunction<I,3>},
    {"function", &UUrbiComponent::execfunction<I,4>},
    {"function", &UUrbiComponent::execfunction<I,5>},
    {"function", &UUrbiComponent::execfunction<I,6>},
    {"function", &UUrbiComponent::execfunction<I,7>},
    {"function", &UUrbiComponent::execfunction<I,8>},
    {"function", &UUrbiComponent::execfunction<I,9>},
    {"function", &UUrbiComponent::execfunction<I,10>},
    {"function", &UUrbiComponent::execfunction<I,11>},
    {"function", &UUrbiComponent::execfunction<I,12>},
    {"function", &UUrbiComponent::execfunction<I,13>},
    {"function", &UUrbiComponent::execfunction<I,14>},
    {"function", &UUrbiComponent::execfunction<I,15>},
    {"function", &UUrbiComponent::execfunction<I,16>},
    {"function", &UUrbiComponent::execfunction<I,17>},
    {"function", &UUrbiComponent::execfunction<I,18>},
    {"function", &UUrbiComponent::execfunction<I,19>},
    {"function", &UUrbiComponent::execfunction<I,20>},
    {"function", &UUrbiComponent::execfunction<I,21>},
    {"function", &UUrbiComponent::execfunction<I,22>},
    {"function", &UUrbiComponent::execfunction<I,23>},
    {"function", &UUrbiComponent::execfunction<I,24>},
    {"function", &UUrbiComponent::execfunction<I,25>},
    {"function", &UUrbiComponent::execfunction<I,26>},
    {"function", &UUrbiComponent::execfunction<I,27>},
    {"function", &UUrbiComponent::execfunction<I,28>},
    {"function", &UUrbiComponent::execfunction<I,29>},
    {"function", &UUrbiComponent::execfunction<I,30>},
    {"function", &UUrbiComponent::execfunction<I,31>},
    {"function", &UUrbiComponent::execfunction<I,32>},
    {"function", &UUrbiComponent::execfunction<I,33>},
    {"function", &UUrbiComponent::execfunction<I,34>},
    {"function", &UUrbiComponent::execfunction<I,35>},
    {"function", &UUrbiComponent::execfunction<I,36>},
    {"function", &UUrbiComponent::execfunction<I,37>},
    {"function", &UUrbiComponent::execfunction<I,38>},
    {"function", &UUrbiComponent::execfunction<I,39>},
    {"function", &UUrbiComponent::execfunction<I,40>},
    {"function", &UUrbiComponent::execfunction<I,41>},
    {"function", &UUrbiComponent::execfunction<I,42>},
    {"function", &UUrbiComponent::execfunction<I,43>},
    {"function", &UUrbiComponent::execfunction<I,44>},
    {"function", &UUrbiComponent::execfunction<I,45>},
    {"function", &UUrbiComponent::execfunction<I,46>},
    {"function", &UUrbiComponent::execfunction<I,47>},
    {"function", &UUrbiComponent::execfunction<I,48>},
    {"function", &UUrbiComponent::execfunction<I,49>},

//END
	};
	for (int f=0; f<gClassModels[I].funcs.size(); f++)
	  Funcs[f].NameUTF8 = gClassModels[I].funcs[f].name.c_str();
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, gClassModels[I].funcs.size());
}

// inner UClass constructor
template<int I> UClass* ConstructInner()
{
	if (gInnerClassPointers[I] != nullptr)
		return gInnerClassPointers[I];
	GetPrivateStaticClassBody(
		TEXT("/Script/orbidyn"),
		**new FString(gClassModels[I].name.c_str())+1,
		gInnerClassPointers[I],
		RegisterNatives<I>,
		sizeof(UUrbiComponent),
		alignof(UUrbiComponent),
		UUrbiComponent::StaticClassFlags,
		UUrbiComponent::StaticClassCastFlags(),
		TEXT("urbiscript component"),
		InternalConstructorHelper<I>,
		InternalVTableHelper<I>,
		UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(UActorComponent),
		&UActorComponent::StaticClass,
		&UUrbiComponent::WithinClass::StaticClass
	);
	return gInnerClassPointers[I];
}

#if WITH_METADATA
const UECodeGen_Private::FMetaDataPairParam GenericMetadataParams[] = {
		{ "IncludePath", "orbi.h" },
		{ "ModuleRelativePath", "Public/orbi.h" },
		{ "BlueprintSpawnableComponent", "" },
		{ "ClassGroupNames", "Misc" },
};
#endif
static const FCppClassTypeInfoStatic gStaticCppClassTypeInfo[MAX_CLASS_COUNT]{};
static UECodeGen_Private::FClassParams gClassParams[MAX_CLASS_COUNT];
static UECodeGen_Private::FPropertyParamsBase* gPropPointers[20][MAX_CLASS_COUNT];

/* Left here just in case, but setters/getters seems to be sometimes
   bypassed, so drop the idea.
template<int I, int J> void Setter(void* obj, const void* val)
{
	UE_LOG(LogScript, Warning, TEXT("SETTER"));
};
template<int I, int J> void Getter(const void* obj, void* val)
{
	UE_LOG(LogScript, Warning, TEXT("GETTER"));
	auto type = gClassModels[I].props[J].type;
	if (type == urbi::DATA_STRING)
		new (val)FString("EMPTY");
	else
		*(double*)val = 42;
};
template<int I> SetterFuncPtr GetSetter(int i)
{
	if (i == 0) return Setter<I, 0>;
	if (i == 1) return Setter<I, 1>;
	if (i == 2) return Setter<I, 2>;
	if (i == 3) return Setter<I, 3>;
	if (i == 4) return Setter<I, 4>;
	if (i == 5) return Setter<I, 5>;
	if (i == 6) return Setter<I, 6>;
	if (i == 7) return Setter<I, 7>;
	if (i == 8) return Setter<I, 8>;
	throw std::runtime_error("bonk");
}
template<int I> GetterFuncPtr GetGetter(int i)
{
	if (i == 0) return Getter<I, 0>;
	if (i == 1) return Getter<I, 1>;
	if (i == 2) return Getter<I, 2>;
	if (i == 3) return Getter<I, 3>;
	if (i == 4) return Getter<I, 4>;
	if (i == 5) return Getter<I, 5>;
	if (i == 6) return Getter<I, 6>;
	if (i == 7) return Getter<I, 7>;
	if (i == 8) return Getter<I, 8>;
	throw std::runtime_error("bonk");
}
*/
#if WITH_METADATA
static const UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
	    { "Category", "Misc" },
	    { "ModuleRelativePath", "Public/orbi.h" },
	  };
#endif

// Construct function F of class I
template<int I, int F> UFunction* ConstructFunction()
{
   if (gFunctions[I][F] != nullptr)
    return gFunctions[I][F];
  std::vector<UECodeGen_Private::FPropertyParamsBase*> uprops;
  uprops = makeUnrealProperties(gClassModels[I].funcs[F].signature);
  memcpy(gFProps[I][F], uprops.data(), uprops.size()*8);
  UECodeGen_Private::FFunctionParams FuncParams = { 
    (UObject*(*)())ConstructInner<I>,
    nullptr, 
    gClassModels[I].funcs[F].name.c_str(),
    nullptr, nullptr,
    gClassModels[I].funcs[F].funcSize(),
    gFProps[I][F],
    uprops.size(),
    RF_Public|RF_Transient|RF_MarkAsNative,
    (EFunctionFlags)0x04020401, 0, 0, 
    METADATA_PARAMS(Function_MetaDataParams,
      UE_ARRAY_COUNT(Function_MetaDataParams))
  };
	
  UECodeGen_Private::ConstructUFunction(&gFunctions[I][F], FuncParams);

  return gFunctions[I][F];
}

FClassFunctionLinkInfo gFuncInfo[MAX_CLASS_COUNT][50] = {
  /*`for i in range(10):
      print('{', end='')h
      for j in range(50):
        print('{ConstructFunction<'+str(i)+','+str(j)+'>, "unnamed"},', end='')
      print('},')
  */
{{ConstructFunction<0,0>, "unnhmed"},{ConstructFunction<0,1>, "unnamed"},{ConstructFunction<0,2>, "unnamed"},{ConstructFunction<0,3>, "unnamed"},{ConstructFunction<0,4>, "unnamed"},{ConstructFunction<0,5>, "unnamed"},{ConstructFunction<0,6>, "unnamed"},{ConstructFunction<0,7>, "unnamed"},{ConstructFunction<0,8>, "unnamed"},{ConstructFunction<0,9>, "unnamed"},{ConstructFunction<0,10>, "unnamed"},{ConstructFunction<0,11>, "unnamed"},{ConstructFunction<0,12>, "unnamed"},{ConstructFunction<0,13>, "unnamed"},{ConstructFunction<0,14>, "unnamed"},{ConstructFunction<0,15>, "unnamed"},{ConstructFunction<0,16>, "unnamed"},{ConstructFunction<0,17>, "unnamed"},{ConstructFunction<0,18>, "unnamed"},{ConstructFunction<0,19>, "unnamed"},{ConstructFunction<0,20>, "unnamed"},{ConstructFunction<0,21>, "unnamed"},{ConstructFunction<0,22>, "unnamed"},{ConstructFunction<0,23>, "unnamed"},{ConstructFunction<0,24>, "unnamed"},{ConstructFunction<0,25>, "unnamed"},{ConstructFunction<0,26>, "unnamed"},{ConstructFunction<0,27>, "unnamed"},{ConstructFunction<0,28>, "unnamed"},{ConstructFunction<0,29>, "unnamed"},{ConstructFunction<0,30>, "unnamed"},{ConstructFunction<0,31>, "unnamed"},{ConstructFunction<0,32>, "unnamed"},{ConstructFunction<0,33>, "unnamed"},{ConstructFunction<0,34>, "unnamed"},{ConstructFunction<0,35>, "unnamed"},{ConstructFunction<0,36>, "unnamed"},{ConstructFunction<0,37>, "unnamed"},{ConstructFunction<0,38>, "unnamed"},{ConstructFunction<0,39>, "unnamed"},{ConstructFunction<0,40>, "unnamed"},{ConstructFunction<0,41>, "unnamed"},{ConstructFunction<0,42>, "unnamed"},{ConstructFunction<0,43>, "unnamed"},{ConstructFunction<0,44>, "unnamed"},{ConstructFunction<0,45>, "unnamed"},{ConstructFunction<0,46>, "unnamed"},{ConstructFunction<0,47>, "unnamed"}},
{{ConstructFunction<1,0>, "unnamed"},{ConstructFunction<1,1>, "unnamed"},{ConstructFunction<1,2>, "unnamed"},{ConstructFunction<1,3>, "unnamed"},{ConstructFunction<1,4>, "unnamed"},{ConstructFunction<1,5>, "unnamed"},{ConstructFunction<1,6>, "unnamed"},{ConstructFunction<1,7>, "unnamed"},{ConstructFunction<1,8>, "unnamed"},{ConstructFunction<1,9>, "unnamed"},{ConstructFunction<1,10>, "unnamed"},{ConstructFunction<1,11>, "unnamed"},{ConstructFunction<1,12>, "unnamed"},{ConstructFunction<1,13>, "unnamed"},{ConstructFunction<1,14>, "unnamed"},{ConstructFunction<1,15>, "unnamed"},{ConstructFunction<1,16>, "unnamed"},{ConstructFunction<1,17>, "unnamed"},{ConstructFunction<1,18>, "unnamed"},{ConstructFunction<1,19>, "unnamed"},{ConstructFunction<1,20>, "unnamed"},{ConstructFunction<1,21>, "unnamed"},{ConstructFunction<1,22>, "unnamed"},{ConstructFunction<1,23>, "unnamed"},{ConstructFunction<1,24>, "unnamed"},{ConstructFunction<1,25>, "unnamed"},{ConstructFunction<1,26>, "unnamed"},{ConstructFunction<1,27>, "unnamed"},{ConstructFunction<1,28>, "unnamed"},{ConstructFunction<1,29>, "unnamed"},{ConstructFunction<1,30>, "unnamed"},{ConstructFunction<1,31>, "unnamed"},{ConstructFunction<1,32>, "unnamed"},{ConstructFunction<1,33>, "unnamed"},{ConstructFunction<1,34>, "unnamed"},{ConstructFunction<1,35>, "unnamed"},{ConstructFunction<1,36>, "unnamed"},{ConstructFunction<1,37>, "unnamed"},{ConstructFunction<1,38>, "unnamed"},{ConstructFunction<1,39>, "unnamed"},{ConstructFunction<1,40>, "unnamed"},{ConstructFunction<1,41>, "unnamed"},{ConstructFunction<1,42>, "unnamed"},{ConstructFunction<1,43>, "unnamed"},{ConstructFunction<1,44>, "unnamed"},{ConstructFunction<1,45>, "unnamed"},{ConstructFunction<1,46>, "unnamed"},{ConstructFunction<1,47>, "unnamed"}},
{{ConstructFunction<2,0>, "unnamed"},{ConstructFunction<2,1>, "unnamed"},{ConstructFunction<2,2>, "unnamed"},{ConstructFunction<2,3>, "unnamed"},{ConstructFunction<2,4>, "unnamed"},{ConstructFunction<2,5>, "unnamed"},{ConstructFunction<2,6>, "unnamed"},{ConstructFunction<2,7>, "unnamed"},{ConstructFunction<2,8>, "unnamed"},{ConstructFunction<2,9>, "unnamed"},{ConstructFunction<2,10>, "unnamed"},{ConstructFunction<2,11>, "unnamed"},{ConstructFunction<2,12>, "unnamed"},{ConstructFunction<2,13>, "unnamed"},{ConstructFunction<2,14>, "unnamed"},{ConstructFunction<2,15>, "unnamed"},{ConstructFunction<2,16>, "unnamed"},{ConstructFunction<2,17>, "unnamed"},{ConstructFunction<2,18>, "unnamed"},{ConstructFunction<2,19>, "unnamed"},{ConstructFunction<2,20>, "unnamed"},{ConstructFunction<2,21>, "unnamed"},{ConstructFunction<2,22>, "unnamed"},{ConstructFunction<2,23>, "unnamed"},{ConstructFunction<2,24>, "unnamed"},{ConstructFunction<2,25>, "unnamed"},{ConstructFunction<2,26>, "unnamed"},{ConstructFunction<2,27>, "unnamed"},{ConstructFunction<2,28>, "unnamed"},{ConstructFunction<2,29>, "unnamed"},{ConstructFunction<2,30>, "unnamed"},{ConstructFunction<2,31>, "unnamed"},{ConstructFunction<2,32>, "unnamed"},{ConstructFunction<2,33>, "unnamed"},{ConstructFunction<2,34>, "unnamed"},{ConstructFunction<2,35>, "unnamed"},{ConstructFunction<2,36>, "unnamed"},{ConstructFunction<2,37>, "unnamed"},{ConstructFunction<2,38>, "unnamed"},{ConstructFunction<2,39>, "unnamed"},{ConstructFunction<2,40>, "unnamed"},{ConstructFunction<2,41>, "unnamed"},{ConstructFunction<2,42>, "unnamed"},{ConstructFunction<2,43>, "unnamed"},{ConstructFunction<2,44>, "unnamed"},{ConstructFunction<2,45>, "unnamed"},{ConstructFunction<2,46>, "unnamed"},{ConstructFunction<2,47>, "unnamed"}},
{{ConstructFunction<3,0>, "unnamed"},{ConstructFunction<3,1>, "unnamed"},{ConstructFunction<3,2>, "unnamed"},{ConstructFunction<3,3>, "unnamed"},{ConstructFunction<3,4>, "unnamed"},{ConstructFunction<3,5>, "unnamed"},{ConstructFunction<3,6>, "unnamed"},{ConstructFunction<3,7>, "unnamed"},{ConstructFunction<3,8>, "unnamed"},{ConstructFunction<3,9>, "unnamed"},{ConstructFunction<3,10>, "unnamed"},{ConstructFunction<3,11>, "unnamed"},{ConstructFunction<3,12>, "unnamed"},{ConstructFunction<3,13>, "unnamed"},{ConstructFunction<3,14>, "unnamed"},{ConstructFunction<3,15>, "unnamed"},{ConstructFunction<3,16>, "unnamed"},{ConstructFunction<3,17>, "unnamed"},{ConstructFunction<3,18>, "unnamed"},{ConstructFunction<3,19>, "unnamed"},{ConstructFunction<3,20>, "unnamed"},{ConstructFunction<3,21>, "unnamed"},{ConstructFunction<3,22>, "unnamed"},{ConstructFunction<3,23>, "unnamed"},{ConstructFunction<3,24>, "unnamed"},{ConstructFunction<3,25>, "unnamed"},{ConstructFunction<3,26>, "unnamed"},{ConstructFunction<3,27>, "unnamed"},{ConstructFunction<3,28>, "unnamed"},{ConstructFunction<3,29>, "unnamed"},{ConstructFunction<3,30>, "unnamed"},{ConstructFunction<3,31>, "unnamed"},{ConstructFunction<3,32>, "unnamed"},{ConstructFunction<3,33>, "unnamed"},{ConstructFunction<3,34>, "unnamed"},{ConstructFunction<3,35>, "unnamed"},{ConstructFunction<3,36>, "unnamed"},{ConstructFunction<3,37>, "unnamed"},{ConstructFunction<3,38>, "unnamed"},{ConstructFunction<3,39>, "unnamed"},{ConstructFunction<3,40>, "unnamed"},{ConstructFunction<3,41>, "unnamed"},{ConstructFunction<3,42>, "unnamed"},{ConstructFunction<3,43>, "unnamed"},{ConstructFunction<3,44>, "unnamed"},{ConstructFunction<3,45>, "unnamed"},{ConstructFunction<3,46>, "unnamed"},{ConstructFunction<3,47>, "unnamed"}},
{{ConstructFunction<4,0>, "unnamed"},{ConstructFunction<4,1>, "unnamed"},{ConstructFunction<4,2>, "unnamed"},{ConstructFunction<4,3>, "unnamed"},{ConstructFunction<4,4>, "unnamed"},{ConstructFunction<4,5>, "unnamed"},{ConstructFunction<4,6>, "unnamed"},{ConstructFunction<4,7>, "unnamed"},{ConstructFunction<4,8>, "unnamed"},{ConstructFunction<4,9>, "unnamed"},{ConstructFunction<4,10>, "unnamed"},{ConstructFunction<4,11>, "unnamed"},{ConstructFunction<4,12>, "unnamed"},{ConstructFunction<4,13>, "unnamed"},{ConstructFunction<4,14>, "unnamed"},{ConstructFunction<4,15>, "unnamed"},{ConstructFunction<4,16>, "unnamed"},{ConstructFunction<4,17>, "unnamed"},{ConstructFunction<4,18>, "unnamed"},{ConstructFunction<4,19>, "unnamed"},{ConstructFunction<4,20>, "unnamed"},{ConstructFunction<4,21>, "unnamed"},{ConstructFunction<4,22>, "unnamed"},{ConstructFunction<4,23>, "unnamed"},{ConstructFunction<4,24>, "unnamed"},{ConstructFunction<4,25>, "unnamed"},{ConstructFunction<4,26>, "unnamed"},{ConstructFunction<4,27>, "unnamed"},{ConstructFunction<4,28>, "unnamed"},{ConstructFunction<4,29>, "unnamed"},{ConstructFunction<4,30>, "unnamed"},{ConstructFunction<4,31>, "unnamed"},{ConstructFunction<4,32>, "unnamed"},{ConstructFunction<4,33>, "unnamed"},{ConstructFunction<4,34>, "unnamed"},{ConstructFunction<4,35>, "unnamed"},{ConstructFunction<4,36>, "unnamed"},{ConstructFunction<4,37>, "unnamed"},{ConstructFunction<4,38>, "unnamed"},{ConstructFunction<4,39>, "unnamed"},{ConstructFunction<4,40>, "unnamed"},{ConstructFunction<4,41>, "unnamed"},{ConstructFunction<4,42>, "unnamed"},{ConstructFunction<4,43>, "unnamed"},{ConstructFunction<4,44>, "unnamed"},{ConstructFunction<4,45>, "unnamed"},{ConstructFunction<4,46>, "unnamed"},{ConstructFunction<4,47>, "unnamed"}},
{{ConstructFunction<5,0>, "unnamed"},{ConstructFunction<5,1>, "unnamed"},{ConstructFunction<5,2>, "unnamed"},{ConstructFunction<5,3>, "unnamed"},{ConstructFunction<5,4>, "unnamed"},{ConstructFunction<5,5>, "unnamed"},{ConstructFunction<5,6>, "unnamed"},{ConstructFunction<5,7>, "unnamed"},{ConstructFunction<5,8>, "unnamed"},{ConstructFunction<5,9>, "unnamed"},{ConstructFunction<5,10>, "unnamed"},{ConstructFunction<5,11>, "unnamed"},{ConstructFunction<5,12>, "unnamed"},{ConstructFunction<5,13>, "unnamed"},{ConstructFunction<5,14>, "unnamed"},{ConstructFunction<5,15>, "unnamed"},{ConstructFunction<5,16>, "unnamed"},{ConstructFunction<5,17>, "unnamed"},{ConstructFunction<5,18>, "unnamed"},{ConstructFunction<5,19>, "unnamed"},{ConstructFunction<5,20>, "unnamed"},{ConstructFunction<5,21>, "unnamed"},{ConstructFunction<5,22>, "unnamed"},{ConstructFunction<5,23>, "unnamed"},{ConstructFunction<5,24>, "unnamed"},{ConstructFunction<5,25>, "unnamed"},{ConstructFunction<5,26>, "unnamed"},{ConstructFunction<5,27>, "unnamed"},{ConstructFunction<5,28>, "unnamed"},{ConstructFunction<5,29>, "unnamed"},{ConstructFunction<5,30>, "unnamed"},{ConstructFunction<5,31>, "unnamed"},{ConstructFunction<5,32>, "unnamed"},{ConstructFunction<5,33>, "unnamed"},{ConstructFunction<5,34>, "unnamed"},{ConstructFunction<5,35>, "unnamed"},{ConstructFunction<5,36>, "unnamed"},{ConstructFunction<5,37>, "unnamed"},{ConstructFunction<5,38>, "unnamed"},{ConstructFunction<5,39>, "unnamed"},{ConstructFunction<5,40>, "unnamed"},{ConstructFunction<5,41>, "unnamed"},{ConstructFunction<5,42>, "unnamed"},{ConstructFunction<5,43>, "unnamed"},{ConstructFunction<5,44>, "unnamed"},{ConstructFunction<5,45>, "unnamed"},{ConstructFunction<5,46>, "unnamed"},{ConstructFunction<5,47>, "unnamed"}},
{{ConstructFunction<6,0>, "unnamed"},{ConstructFunction<6,1>, "unnamed"},{ConstructFunction<6,2>, "unnamed"},{ConstructFunction<6,3>, "unnamed"},{ConstructFunction<6,4>, "unnamed"},{ConstructFunction<6,5>, "unnamed"},{ConstructFunction<6,6>, "unnamed"},{ConstructFunction<6,7>, "unnamed"},{ConstructFunction<6,8>, "unnamed"},{ConstructFunction<6,9>, "unnamed"},{ConstructFunction<6,10>, "unnamed"},{ConstructFunction<6,11>, "unnamed"},{ConstructFunction<6,12>, "unnamed"},{ConstructFunction<6,13>, "unnamed"},{ConstructFunction<6,14>, "unnamed"},{ConstructFunction<6,15>, "unnamed"},{ConstructFunction<6,16>, "unnamed"},{ConstructFunction<6,17>, "unnamed"},{ConstructFunction<6,18>, "unnamed"},{ConstructFunction<6,19>, "unnamed"},{ConstructFunction<6,20>, "unnamed"},{ConstructFunction<6,21>, "unnamed"},{ConstructFunction<6,22>, "unnamed"},{ConstructFunction<6,23>, "unnamed"},{ConstructFunction<6,24>, "unnamed"},{ConstructFunction<6,25>, "unnamed"},{ConstructFunction<6,26>, "unnamed"},{ConstructFunction<6,27>, "unnamed"},{ConstructFunction<6,28>, "unnamed"},{ConstructFunction<6,29>, "unnamed"},{ConstructFunction<6,30>, "unnamed"},{ConstructFunction<6,31>, "unnamed"},{ConstructFunction<6,32>, "unnamed"},{ConstructFunction<6,33>, "unnamed"},{ConstructFunction<6,34>, "unnamed"},{ConstructFunction<6,35>, "unnamed"},{ConstructFunction<6,36>, "unnamed"},{ConstructFunction<6,37>, "unnamed"},{ConstructFunction<6,38>, "unnamed"},{ConstructFunction<6,39>, "unnamed"},{ConstructFunction<6,40>, "unnamed"},{ConstructFunction<6,41>, "unnamed"},{ConstructFunction<6,42>, "unnamed"},{ConstructFunction<6,43>, "unnamed"},{ConstructFunction<6,44>, "unnamed"},{ConstructFunction<6,45>, "unnamed"},{ConstructFunction<6,46>, "unnamed"},{ConstructFunction<6,47>, "unnamed"}},
{{ConstructFunction<7,0>, "unnamed"},{ConstructFunction<7,1>, "unnamed"},{ConstructFunction<7,2>, "unnamed"},{ConstructFunction<7,3>, "unnamed"},{ConstructFunction<7,4>, "unnamed"},{ConstructFunction<7,5>, "unnamed"},{ConstructFunction<7,6>, "unnamed"},{ConstructFunction<7,7>, "unnamed"},{ConstructFunction<7,8>, "unnamed"},{ConstructFunction<7,9>, "unnamed"},{ConstructFunction<7,10>, "unnamed"},{ConstructFunction<7,11>, "unnamed"},{ConstructFunction<7,12>, "unnamed"},{ConstructFunction<7,13>, "unnamed"},{ConstructFunction<7,14>, "unnamed"},{ConstructFunction<7,15>, "unnamed"},{ConstructFunction<7,16>, "unnamed"},{ConstructFunction<7,17>, "unnamed"},{ConstructFunction<7,18>, "unnamed"},{ConstructFunction<7,19>, "unnamed"},{ConstructFunction<7,20>, "unnamed"},{ConstructFunction<7,21>, "unnamed"},{ConstructFunction<7,22>, "unnamed"},{ConstructFunction<7,23>, "unnamed"},{ConstructFunction<7,24>, "unnamed"},{ConstructFunction<7,25>, "unnamed"},{ConstructFunction<7,26>, "unnamed"},{ConstructFunction<7,27>, "unnamed"},{ConstructFunction<7,28>, "unnamed"},{ConstructFunction<7,29>, "unnamed"},{ConstructFunction<7,30>, "unnamed"},{ConstructFunction<7,31>, "unnamed"},{ConstructFunction<7,32>, "unnamed"},{ConstructFunction<7,33>, "unnamed"},{ConstructFunction<7,34>, "unnamed"},{ConstructFunction<7,35>, "unnamed"},{ConstructFunction<7,36>, "unnamed"},{ConstructFunction<7,37>, "unnamed"},{ConstructFunction<7,38>, "unnamed"},{ConstructFunction<7,39>, "unnamed"},{ConstructFunction<7,40>, "unnamed"},{ConstructFunction<7,41>, "unnamed"},{ConstructFunction<7,42>, "unnamed"},{ConstructFunction<7,43>, "unnamed"},{ConstructFunction<7,44>, "unnamed"},{ConstructFunction<7,45>, "unnamed"},{ConstructFunction<7,46>, "unnamed"},{ConstructFunction<7,47>, "unnamed"}},
{{ConstructFunction<8,0>, "unnamed"},{ConstructFunction<8,1>, "unnamed"},{ConstructFunction<8,2>, "unnamed"},{ConstructFunction<8,3>, "unnamed"},{ConstructFunction<8,4>, "unnamed"},{ConstructFunction<8,5>, "unnamed"},{ConstructFunction<8,6>, "unnamed"},{ConstructFunction<8,7>, "unnamed"},{ConstructFunction<8,8>, "unnamed"},{ConstructFunction<8,9>, "unnamed"},{ConstructFunction<8,10>, "unnamed"},{ConstructFunction<8,11>, "unnamed"},{ConstructFunction<8,12>, "unnamed"},{ConstructFunction<8,13>, "unnamed"},{ConstructFunction<8,14>, "unnamed"},{ConstructFunction<8,15>, "unnamed"},{ConstructFunction<8,16>, "unnamed"},{ConstructFunction<8,17>, "unnamed"},{ConstructFunction<8,18>, "unnamed"},{ConstructFunction<8,19>, "unnamed"},{ConstructFunction<8,20>, "unnamed"},{ConstructFunction<8,21>, "unnamed"},{ConstructFunction<8,22>, "unnamed"},{ConstructFunction<8,23>, "unnamed"},{ConstructFunction<8,24>, "unnamed"},{ConstructFunction<8,25>, "unnamed"},{ConstructFunction<8,26>, "unnamed"},{ConstructFunction<8,27>, "unnamed"},{ConstructFunction<8,28>, "unnamed"},{ConstructFunction<8,29>, "unnamed"},{ConstructFunction<8,30>, "unnamed"},{ConstructFunction<8,31>, "unnamed"},{ConstructFunction<8,32>, "unnamed"},{ConstructFunction<8,33>, "unnamed"},{ConstructFunction<8,34>, "unnamed"},{ConstructFunction<8,35>, "unnamed"},{ConstructFunction<8,36>, "unnamed"},{ConstructFunction<8,37>, "unnamed"},{ConstructFunction<8,38>, "unnamed"},{ConstructFunction<8,39>, "unnamed"},{ConstructFunction<8,40>, "unnamed"},{ConstructFunction<8,41>, "unnamed"},{ConstructFunction<8,42>, "unnamed"},{ConstructFunction<8,43>, "unnamed"},{ConstructFunction<8,44>, "unnamed"},{ConstructFunction<8,45>, "unnamed"},{ConstructFunction<8,46>, "unnamed"},{ConstructFunction<8,47>, "unnamed"}},
{{ConstructFunction<9,0>, "unnamed"},{ConstructFunction<9,1>, "unnamed"},{ConstructFunction<9,2>, "unnamed"},{ConstructFunction<9,3>, "unnamed"},{ConstructFunction<9,4>, "unnamed"},{ConstructFunction<9,5>, "unnamed"},{ConstructFunction<9,6>, "unnamed"},{ConstructFunction<9,7>, "unnamed"},{ConstructFunction<9,8>, "unnamed"},{ConstructFunction<9,9>, "unnamed"},{ConstructFunction<9,10>, "unnamed"},{ConstructFunction<9,11>, "unnamed"},{ConstructFunction<9,12>, "unnamed"},{ConstructFunction<9,13>, "unnamed"},{ConstructFunction<9,14>, "unnamed"},{ConstructFunction<9,15>, "unnamed"},{ConstructFunction<9,16>, "unnamed"},{ConstructFunction<9,17>, "unnamed"},{ConstructFunction<9,18>, "unnamed"},{ConstructFunction<9,19>, "unnamed"},{ConstructFunction<9,20>, "unnamed"},{ConstructFunction<9,21>, "unnamed"},{ConstructFunction<9,22>, "unnamed"},{ConstructFunction<9,23>, "unnamed"},{ConstructFunction<9,24>, "unnamed"},{ConstructFunction<9,25>, "unnamed"},{ConstructFunction<9,26>, "unnamed"},{ConstructFunction<9,27>, "unnamed"},{ConstructFunction<9,28>, "unnamed"},{ConstructFunction<9,29>, "unnamed"},{ConstructFunction<9,30>, "unnamed"},{ConstructFunction<9,31>, "unnamed"},{ConstructFunction<9,32>, "unnamed"},{ConstructFunction<9,33>, "unnamed"},{ConstructFunction<9,34>, "unnamed"},{ConstructFunction<9,35>, "unnamed"},{ConstructFunction<9,36>, "unnamed"},{ConstructFunction<9,37>, "unnamed"},{ConstructFunction<9,38>, "unnamed"},{ConstructFunction<9,39>, "unnamed"},{ConstructFunction<9,40>, "unnamed"},{ConstructFunction<9,41>, "unnamed"},{ConstructFunction<9,42>, "unnamed"},{ConstructFunction<9,43>, "unnamed"},{ConstructFunction<9,44>, "unnamed"},{ConstructFunction<9,45>, "unnamed"},{ConstructFunction<9,46>, "unnamed"},{ConstructFunction<9,47>, "unnamed"}}

//END
};

// Construct outer UClass of an urbiscript component
template<int I> UClass* ConstructOuter()
{
	if (gOuterClassPointers[I] != nullptr)
		return gOuterClassPointers[I];
	auto props = gPropPointers[I];
	int propCount = gClassModels[I].props.size();
	int coffset = offsetof(UUrbiComponent, buffer);

	EPropertyFlags pf = (EPropertyFlags)0x0010000000000001;
	EPropertyFlags pfa = (EPropertyFlags)0x0000000000000000;
	int pIndex = 0;
	for (int i = 0; i < propCount; i++)
	{
	  auto& p = gClassModels[I].props[i];
		auto type = gClassModels[I].props[i].type;
		auto isPtr = gClassModels[I].props[i].isPtr;
		int propSize = 32;
		UECodeGen_Private::EPropertyGenFlags utype = UECodeGen_Private::EPropertyGenFlags::Double;
		if (isPtr)
			utype = UECodeGen_Private::EPropertyGenFlags::Object;
		else if (type == urbi::DATA_DOUBLE)
			utype = UECodeGen_Private::EPropertyGenFlags::Double;
		else if (type == urbi::DATA_STRING)
			utype = UECodeGen_Private::EPropertyGenFlags::Str;
		else if (type == urbi::DATA_LIST)
			utype = UECodeGen_Private::EPropertyGenFlags::Array;
		else
		  continue;
		if (utype == UECodeGen_Private::EPropertyGenFlags::Array)
		{
			auto itype = gClassModels[I].props[i].innerType;
			auto iisPtr = gClassModels[I].props[i].isInnerPtr;
			UECodeGen_Private::EPropertyGenFlags iutype;
			if (iisPtr)
				iutype = UECodeGen_Private::EPropertyGenFlags::Object;
			else if (itype == urbi::DATA_DOUBLE)
				iutype = UECodeGen_Private::EPropertyGenFlags::Double;
			else if (itype == urbi::DATA_STRING)
				iutype = UECodeGen_Private::EPropertyGenFlags::Str;
			else continue;
			if (iisPtr)
				props[pIndex++] = (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FObjectPropertyParams(UECodeGen_Private::FObjectPropertyParams
				{
				gClassModels[I].props[i].name.c_str(),
					nullptr,
					pfa,
					iutype,
					RF_Public | RF_Transient | RF_MarkAsNative,
					1,
					nullptr, // GetSetter<I>(i),
					nullptr, //GetGetter<I>(i),
					0,
					Z_Construct_UClass_UObject_NoRegister,
					METADATA_PARAMS(GenericPropMeta, 2)
				});
			else if (p.innerStructName.length())
			{
			  props[pIndex++] = ((UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FStructPropertyParams(UECodeGen_Private::FStructPropertyParams
				{
					  p.name.c_str(),
						nullptr,
						pfa,
						UECodeGen_Private::EPropertyGenFlags::Struct,
						RF_Public | RF_Transient | RF_MarkAsNative,
						1,
						nullptr, //GetSetter<I>(i),
						nullptr, //GetGetter<I>(i),
						0,
						GenRecordKnownStructConstructor(p.innerStructName),
						METADATA_PARAMS(GenericPropMeta, 2)
				}));
			}
			else
				props[pIndex++] = (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FGenericPropertyParams(UECodeGen_Private::FGenericPropertyParams
				{
					gClassModels[I].props[i].name.c_str(),
						nullptr,
						pfa,
						iutype,
						RF_Public | RF_Transient | RF_MarkAsNative,
						1,
						nullptr, //GetSetter<I>(i),
						nullptr, //GetGetter<I>(i),
						0,
						METADATA_PARAMS(GenericPropMeta, 2)
				});
			props[pIndex++] = (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FArrayPropertyParams(UECodeGen_Private::FArrayPropertyParams
			{
				gClassModels[I].props[i].name.c_str(),
				nullptr,
				pf,
				utype,
				RF_Public | RF_Transient | RF_MarkAsNative,
				1,
				nullptr, //GetSetter<I>(i),
				nullptr, //GetGetter<I>(i),
				coffset,
				EArrayPropertyFlags::None,
				METADATA_PARAMS(GenericPropMeta, 2)
			});
		}
		else
		{
			if (isPtr)
				props[pIndex++] = (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FObjectPropertyParams(UECodeGen_Private::FObjectPropertyParams
				{
					gClassModels[I].props[i].name.c_str(),
					nullptr,
					pf,
					utype,
					RF_Public | RF_Transient | RF_MarkAsNative,
					1,
					nullptr, //GetSetter<I>(i),
					nullptr, //GetGetter<I>(i),
					coffset,
					Z_Construct_UClass_UObject_NoRegister,
					METADATA_PARAMS(GenericPropMeta, 2)
				});
			else if (p.structName.length())
			{
			  ClassPropModel cpm;
			  props[pIndex++] = ((UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FStructPropertyParams(UECodeGen_Private::FStructPropertyParams
				{
					  p.name.c_str(),
						nullptr,
						pf,
						UECodeGen_Private::EPropertyGenFlags::Struct,
						RF_Public | RF_Transient | RF_MarkAsNative,
						1,
						nullptr, //GetSetter<I>(i),
						nullptr, //GetGetter<I>(i),
						coffset,
						GenRecordKnownStructConstructor(p.structName, &cpm),
						METADATA_PARAMS(GenericPropMeta, 2)
				}));
				propSize = cpm.size;
				if (propSize % 8)
				{
				  propSize += 8 - (propSize%8);
				}
			}
			else
				props[pIndex++] = (UECodeGen_Private::FPropertyParamsBase*)new UECodeGen_Private::FGenericPropertyParams(UECodeGen_Private::FGenericPropertyParams
				{
					gClassModels[I].props[i].name.c_str(),
					nullptr,
					pf,
					utype,
					RF_Public | RF_Transient | RF_MarkAsNative,
					1,
					nullptr, //GetSetter<I>(i),
					nullptr, //GetGetter<I>(i),
					coffset,
					METADATA_PARAMS(GenericPropMeta, 2)
				});
		}
		gClassModels[I].props[i].offset = coffset;
		coffset += propSize;
	}
	for (int f = 0; f < gClassModels[I].funcs.size(); f++)
	{
	  gFuncInfo[I][f].FuncNameUTF8 = gClassModels[I].funcs[f].name.c_str();
	}
	gClassParams[I] = UECodeGen_Private::FClassParams{
		ConstructInner<I>,
		"Engine",
		&gStaticCppClassTypeInfo[I],
		0,//DependentSingletons,
		gFuncInfo[I], //funcs
		props,
		nullptr,
		0,//UE_ARRAY_COUNT(DependentSingletons),
		(int)gClassModels[I].funcs.size(), //UE_ARRAY_COUNT(FuncInfo),
		pIndex,
		0,
		0x008000A4u,
		METADATA_PARAMS(GenericMetadataParams, UE_ARRAY_COUNT(GenericMetadataParams))
	};
	UECodeGen_Private::ConstructUClass(gOuterClassPointers[I], gClassParams[I]);
	return gOuterClassPointers[I];
}


static UPackage* daPackage = nullptr;
UPackage* ConstructPackage()
{
	if (!daPackage)
	{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/orbidyn",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000000,
			0xD5407DB7,
			0x1543759E,
			METADATA_PARAMS(nullptr, 0)
		};
		UECodeGen_Private::ConstructUPackage(daPackage, PackageParams);
	}
	return daPackage;
}
FClassRegisterCompiledInInfo gCompiledClassInfos[MAX_CLASS_COUNT] = {
	{ &ConstructOuter<0>, &ConstructInner<0>, TEXT("notinited")},
	{ &ConstructOuter<1>, &ConstructInner<1>, TEXT("notinited")},
	{ &ConstructOuter<2>, &ConstructInner<2>, TEXT("notinited")},
	{ &ConstructOuter<3>, &ConstructInner<3>, TEXT("notinited")},
	{ &ConstructOuter<4>, &ConstructInner<4>, TEXT("notinited")},
	{ &ConstructOuter<5>, &ConstructInner<5>, TEXT("notinited")},
	{ &ConstructOuter<6>, &ConstructInner<6>, TEXT("notinited")},
	{ &ConstructOuter<7>, &ConstructInner<7>, TEXT("notinited")},
	{ &ConstructOuter<8>, &ConstructInner<8>, TEXT("notinited")},
	{ &ConstructOuter<9>, &ConstructInner<9>, TEXT("notinited")},
};

// Effective registration of UClass-es for urbiscript components
void DoReload()
{
	memset(gInnerClassPointers, 0, sizeof(gInnerClassPointers));
	memset(gOuterClassPointers, 0, sizeof(gOuterClassPointers));
	memset(gFunctions, 0, sizeof(gFunctions));
	gGenerationCounter++;
	static unsigned int magicVersion = 0;
	for (int i = 0; i < gClassCount; i++)
	{
		gCompiledClassInfos[i].Name = **new FString(gClassModels[i].name.c_str());
		UE_LOG(LogScript, Warning, TEXT("WILL REGISTER %s"), gCompiledClassInfos[i].Name);
		gCompiledClassInfos[i].Info = new FClassRegistrationInfo();
		gCompiledClassInfos[i].VersionInfo = CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, 8, ++magicVersion);
	};
	/* do we still need that?
	auto* p = FindPackage(nullptr, TEXT("/Script/orbidyn"));
	if (p == nullptr)
	{
		static FPackageRegistrationInfo Registration;
		CreatePackage(TEXT("/Script/orbidyn"));
		RegisterCompiledInInfo(ConstructPackage, TEXT("/Script/orbidyn"), Registration, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xD5407D87, 0x1543759F));
		UObjectForceRegistration(ConstructPackage(), false);
		ELoadFlags fl = LOAD_FindIfFail; //  ELoadFlags::LOAD_None;
		LoadPackage(nullptr, TEXT("/Script/orbidyn"), fl, nullptr, nullptr);
	}*/
	RegisterCompiledInInfo(TEXT("/Script/orbidyn"), gCompiledClassInfos, gClassCount, nullptr, 0, nullptr, 0);
	//needed?ProcessNewlyLoadedUObjects(TEXT("/Script/orbidyn"), true);
}
urbi::UDictionary parseClasses();

// Parse an urbiscript value and convert into a ClassPropModel
ClassPropModel parseProp(urbi::UValue const& v)
{
  ClassPropModel res;
  res.type = v.type;
  if (v.type == urbi::DATA_STRING)
  {
    auto s = (std::string)v;
    if (s.length() && s[0] == '$')
      res.isPtr = true;
    else if (s.length())
      res.structName = s;
  }
  if (v.type == urbi::DATA_LIST)
  {
    auto inner = (urbi::UList)v;
    if (inner.size() == 0)
    {
      res.valid = false;
      return res;
    }
    res.innerType = inner[0].type;
    if (res.innerType == urbi::DATA_STRING)
    {
      auto s = (std::string)inner[0];
      if (s.length() && s[0] == '$')
        res.isInnerPtr = true;
      else if (s.length())
        res.innerStructName = s;
    }
  }
  return res;
}

// urbi->reload menu pressed
static void ReloadButton()
{
  	auto dict = parseClasses();
}

// Loads generated 'orbi.urbi' class definitions, parse it, generate static
// introspection data and load it.
static void UrbiReload()
{
  std::ifstream ifs(str(FPaths::ProjectDir()) + "/Content/urbi/orbi.urbi");
  if (!ifs.good())
  {
    UE_LOG(LogScript, Error, TEXT("Failed to load orbi class definitions"));
    return;
  }
  std::string urbivalue(std::istreambuf_iterator<char>(ifs), {});
  UE_LOG(LogScript, Warning, TEXT("loaded orbi classes: %s"), *FString(urbivalue.c_str()));
  urbi::UValue d;
  const urbi::binaries_type bins;
  auto bit = bins.begin();
  int endp = d.parse(urbivalue.c_str(), 0, bins, bit);
  if (endp < 0)
  {
    UE_LOG(LogScript, Error, TEXT("Failed to parse orbi class definitions"));
    return;
  }
  auto& dict = *d.dictionary;
	gOldClassCount = gClassCount;
	gClassCount = dict.size();
	int i = 0;
	for (auto& de: dict)
	{
		gClassModels[i].name = de.first;
		gClassModels[i].props.clear();
		gClassModels[i].funcs.clear();
		auto pf = (urbi::UList)de.second;
		auto l = (urbi::UList)pf[0];
		for (int j = 0; j < l.size(); j++)
		{
		  ClassPropModel cpm;
			auto nv = (urbi::UList)l[j];
			cpm.name = (std::string)nv[0];
			cpm.type = nv[1].type;
			if (nv[1].type == urbi::DATA_STRING)
			{
				auto s = (std::string)nv[1];
				if (s.length() && s[0] == '$')
					cpm.isPtr = true;
				else if (s.length())
				  cpm.structName = s;
			}
			if (nv[1].type == urbi::DATA_LIST)
			{
				auto inner = (urbi::UList)nv[1];
				if (inner.size() == 0)
					continue;
				cpm.innerType = inner[0].type;
				if (cpm.innerType == urbi::DATA_STRING)
				{
					auto s = (std::string)inner[0];
					if (s.length() && s[0] == '$')
						cpm.isInnerPtr = true;
					else if (s.length())
					  cpm.innerStructName = s;
				}
			}
			gClassModels[i].props.push_back(cpm);
		}
		l = (urbi::UList)pf[1];
		for (int j = 0; j < l.size(); j++)
		{
		  urbi::UList f = (urbi::UList)l[j]; // name, [sig]
		  ClassFuncModel cfm;
		  cfm.name = (std::string)f[0];
		  urbi::UList args = (urbi::UList)f[1];
		  bool ok = true;
		  for (int k=0; k < args.size(); k++)
		  {
		    auto p = parseProp(args[k]);
		    if (!p.valid)
		    {
		      ok = false;
		      break;
		    }
		    p.name = "arg" + std::to_string(k+1);
		    if (k == args.size()-1)
		      p.name = "ReturnValue";
		    cfm.signature.push_back(p);
		  }
		  if (ok)
		    gClassModels[i].funcs.push_back(cfm);
		}
		i++;
	}
	if (dict.size() == 0)
		return;
	/*
  UClass* oldInnerClassPointers[MAX_CLASS_COUNT];
  UClass* oldOuterClassPointers[MAX_CLASS_COUNT];
  memcpy(oldInnerClassPointers, gInnerClassPointers, sizeof(gInnerClassPointers));
  memcpy(oldOuterClassPointers, gOuterClassPointers, sizeof(gInnerClassPointers));
	*/
	DoReload();
	/* Nice Dream, but unreal asserts with "can't do outside of hot reload gnagna"
	for (int o=0; o<gOldClassCount; o++)
	{
	  for (int j=0; j<gClassCount; j++)
	  {
	    if (gInnerClassPointers[j]->GetName() == oldInnerClassPointers[o]->GetName())
	    {
	      FReplaceInstancesOfClassParameters params;
	      params.bClassObjectReplaced = true;
	      FBlueprintCompileReinstancer::ReplaceInstancesOfClass(oldInnerClassPointers[o], gInnerClassPointers[j],
	        params);
	    }
	  }
	}
	*/
}

// Attempt at hot-reloading, not used for now
static bool first_time = true;
extern "C" __attribute__((visibility("default"))) void orbi_hook()
{
  if (first_time)
  {
    first_time = false;
    return;
  }
  if (getenv("ORBI_NO_RELOAD_HOOK"))
    return;
  UrbiReload();
}

// Setup introspection at static initialization time. Later and we
// break dependant blueprints.
static int bootTimeReload()
{
  if (!getenv("ORBI_NO_BOOT")) // safety to be able to load plugin even if broken
    UrbiReload();
  return 42;
}

static int unused = bootTimeReload();

// Unbox an uobject pointer from urbiscript
template<typename T = ::UObject>
inline T* unbox(std::string const& sbptr)
{
	auto sptr = sbptr.substr(1);
	auto lptr = std::atoll(sptr.c_str());
	void* ptr = (void*)lptr;
	return (T*)ptr;
}
// box an uobject pointer to urbiscript
inline std::string box(void* ptr)
{
	return "$" + std::to_string((uint64)ptr);
}

// Register our menu entry
void FurbiModule::StartupModule()
{
  #if WITH_EDITOR
  FLevelEditorModule& LevelEditorModule =
  FModuleManager::LoadModuleChecked<FLevelEditorModule>
  ("LevelEditor");
  TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
  
  MenuExtender->AddMenuBarExtension(
    "Help",
    EExtensionHook::After,
    nullptr,
    FMenuBarExtensionDelegate::CreateRaw(this, &FurbiModule::AddMenu)
    );
  LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
  #endif
}

#if WITH_EDITOR
void FurbiModule::AddMenu(FMenuBarBuilder& MenuBuilder)
{
	MenuBuilder.AddPullDownMenu(
		LOCTEXT("UrbiMenuLocKey", "Urbi"),
		LOCTEXT("UrbiMenuTooltipKey", "Opens menu for Urbi"),
		FNewMenuDelegate::CreateRaw(this, &FurbiModule::FillMenu),
		FName(TEXT("Urbi Menu")),
		FName(TEXT("UrbiMenu")));
}
void FurbiModule::FillMenu(FMenuBuilder& MenuBuilder)
{
	FUIAction reloadAction(FExecuteAction::CreateStatic(&ReloadButton), EUIActionRepeatMode::RepeatDisabled );
	MenuBuilder.AddMenuEntry(FText::FromString(TEXT("Reload")), FText::FromString(TEXT("Reload urbi")), FSlateIcon(), reloadAction);
}
#endif

// Bounce all urbiscript produced messages to UE_LOG for platforms without stdout access
static void onUrbiMessage(const char* buf, size_t len)
{
	UE_LOG(LogScript, Warning, TEXT("%s"), *FString(buf, len));
}

// Spawn urbiscript and execute classparse.u that will generate a UValue
// with class, props and functions data.
// Save it in 'orbi.urbi' file, that will be loaded at next boot.
urbi::UDictionary parseClasses()
{
	//urbi::poll_control(false);
	std::vector<std::string> args;
	args.push_back("urbi");
	urbi::init_kernel(
#ifdef _MSC_VER
  "c:"
#endif
  "/bury/transient/urbi-static", args);
	urbi::set_ghost_mirror(&onUrbiMessage);
	//NO OR BOOM urbi::load_file(str(FPaths::ProjectDir() + TEXT("/unreal.u")));
	urbi::load_file(str(FPaths::ProjectDir() + TEXT("/Content/urbi/classparse.u"))); int cnt = 0;
	while (cnt < 10000)
	{
		cnt++;
		urbi::step_kernel();
		urbi::UValue& vl = unrealInstance->parseResult;
		if (vl.type == urbi::DATA_STRING)
		{
			UE_LOG(LogScript, Error, TEXT("Parse failure: %s"), *FString(((std::string)vl).c_str()));
			urbi::kill_kernel();
			return urbi::UDictionary();
		}
		else if (vl.type == urbi::DATA_DICTIONARY)
		{
			urbi::kill_kernel();
			std::ofstream ofs(str(FPaths::ProjectDir()) + "/Content/urbi/orbi.urbi");
			ofs << unrealInstance->parseResultString;
			return vl;
		}
	}
	UE_LOG(LogScript, Error, TEXT("Timeout parsing classes"));
	return urbi::UDictionary();
}

UPackage* ConstructPackage();


static void UrbiReload();

void FurbiModule::ShutdownModule()
{
	//urbi::kill_kernel();
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}
void FurbiModule::sampleFunctionOne()
{

}

void FurbiModule::sampleFunctionTwo(int i)
{

}
static UWorld* world = nullptr;
static bool worldInit = false;
static bool dead = false;
static bool urbi_loaded = false;
static bool urbi_ready = false;
AUrbiBridge* AUrbiBridge::instance;

urbi::UValue FUrbiscriptValue::toUValue() const
{
  if (longValue != UNSET_LONG)
    return urbi::UValue(longValue);
  if (doubleValue != UNSET_DOUBLE)
    return urbi::UValue(doubleValue);
  if (stringValue != FString(TEXT("<<urbiscript unset>>")))
  {
    return urbi::UValue(str(stringValue));
  }
  urbi::UList res;
  for (auto const& e: listValue)
  {
    res.push_back(e.toUValue());
  }
  return urbi::UValue(res);
}

FUrbiscriptValue FUrbiscriptValue::fromUValue(urbi::UValue const& uv)
{
  FUrbiscriptValue res;
  if (uv.type == urbi::DATA_DOUBLE)
  {
    res.doubleValue = (double)uv;
    res.longValue = (long)uv;
  }
  else if (uv.type == urbi::DATA_STRING)
    res.stringValue = FString(((std::string)uv).c_str());
  else if (uv.type == urbi::DATA_LIST)
  {
    urbi::UList l = (urbi::UList)uv;
    for (urbi::UValue* lv: l)
    {
      auto next = FUrbiscriptValue::fromUValue(*lv);
      res.listValue.Add(next);
    }
  }
  return res;
}


AUrbiBridge::AUrbiBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true; 
	instance = this;

}
void AUrbiBridge::EndPlay(EEndPlayReason::Type reason)
{
  
  dead = true;
  try
  {
    urbi::kill_kernel();
	urbi_loaded = false;
  }
  catch(std::exception const& e)
  {
    std::cerr << "exception killing kernel: " << e.what() << std::endl;
  }
  
  UWrapper::Instance = nullptr;
  worldInit = false;
  if (unrealInstance != nullptr)
    unrealInstance->wipeCache();
}
void AUrbiBridge::BeginPlay()
{
    Super::BeginPlay();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	dead = false;
	urbi_ready = false;
}


void AUrbiBridge::Tick(float DeltaTime)
{
  if (dead)
  {
    return;
  };
	if (UWrapper::Instance == nullptr)
	{
		UWrapper::Instance = NewObject<UWrapper>();
		wrappers.Push(UWrapper::Instance); // stupid GC, I lost like 3 hours
	}
	auto start = libport::time::now();
   Super::Tick(DeltaTime);
   //if (!worldInit)
   {
		worldInit = true;
		world = GetWorld();
   }
   if (/*enabled &&*/ !urbi_loaded)
   {
	urbi_loaded = true;
	std::vector<std::string> args;
	args.push_back("urbi");
	if (ListenPort >= 0)
	{
	  args.push_back("--port=" + std::to_string(ListenPort));
	}
	//urbi::poll_control(true);
   urbi::init_kernel(
#ifdef _MSC_VER
  "c:"
#endif
   "/bury/transient/urbi-static", args);
	 urbi::set_ghost_mirror(&onUrbiMessage);
	 urbi::load_file(str(FPaths::ProjectDir() + TEXT("/Content/urbi/unreal.u")));
	 auto mainu = str(FPaths::ProjectDir() + TEXT("/Content/urbi/main.u"));
	 std::ifstream ifs(mainu);
	 if (ifs.good())
		 urbi::load_file(mainu);
	 urbi_ready = true;
   }
   libport::Duration budget = libport::time::us(BudgetMicroseconds);
   if (/*enabled &&*/ urbi_loaded)
   {
	   static libport::Statistics<double> statCount(1000);
	   static libport::Statistics<double> statTime(1000);
     unsigned long next = 0;
     auto tstart = libport::utime();
     auto count = 0;
     urbi::set_update_tick(true);
     do
     {
       next = urbi::step_kernel();
       urbi::set_update_tick(false);
       count++;
     }
     while (next == 0
       && libport::time::now()-start < budget
       && count < BudgetTicks);
     auto dur = libport::utime() - tstart;
     statCount.add_sample(count);
     statTime.add_sample(dur);
     if (statTime.size() >= 1000)
     {
       UE_LOG(LogScript, Warning, TEXT("URBI TIME max %d avg %d"), (uint64)statTime.max(), (uint64)statTime.mean());
       UE_LOG(LogScript, Warning, TEXT("URBI COUNT max %d avg %d"), (uint64)statCount.max(), (uint64)statCount.mean());
       statTime.resize(1000);
       statCount.resize(1000);
     }
   }
   auto end = libport::time::now();
}

// Structs to hold pending calls to urbiscript, since we need to be
// inside the urbi scheduler to execute anything.
static int64 nextCallUid = 1;
struct PendingCall
{
  int64 uid;
  std::string functionName;
  TArray<FUrbiscriptValue> args;
  urbi::UValue nativeArgs;
};
struct PendingResult
{
  int64 uid;
  FUrbiscriptValue result;
  urbi::UValue nativeResult;
};

static std::vector<PendingCall> pendingCalls;
static std::vector<PendingResult> pendingResults;
static std::vector<std::string> executeUrbiscript;
static std::vector<std::pair<std::string, urbi::UValue>> pendingWrites;

FUrbiscriptValue AUrbiBridge::addToList(FUrbiscriptValue lst, FUrbiscriptValue value)
{
  lst.listValue.Add(value);
  return lst;
}

FUrbiscriptValue AUrbiBridge::makeListValue(TArray<FUrbiscriptValue> vals)
{
  FUrbiscriptValue res;
  res.listValue = vals;
  return res;
}
FUrbiscriptValue AUrbiBridge::makeStringValue(FString val)
{
  FUrbiscriptValue res;
  res.stringValue  = val;
  return res;
}
FUrbiscriptValue AUrbiBridge::makeLongValue(int64 val)
{
  FUrbiscriptValue res;
  res.longValue = val;
  return res;
}
FUrbiscriptValue AUrbiBridge::makeFloatValue(double val)
{
  FUrbiscriptValue res;
  res.doubleValue = val;
  return res;
}
TArray<FUrbiscriptValue> AUrbiBridge::getValueFieldList(FUrbiscriptValue val)
{
  return val.listValue;
}

int64 AUrbiBridge::callUrbi(FString functionName, TArray<FUrbiscriptValue> const& args)
{
  PendingCall pc;
  pc.uid = ++nextCallUid;
  pc.functionName = str(functionName);
  pc.args = args;
  pendingCalls.push_back(pc);
  return pc.uid;
}

bool AUrbiBridge::hasCallResult(int64 uid)
{
  for (auto const& r: pendingResults)
  {
    if (r.uid == uid)
      return true;
  }
  return false;
}

FUrbiscriptValue AUrbiBridge::getCallResult(int64 uid)
{
  int index = 0;
  for (auto const& r: pendingResults)
  {
    if (r.uid == uid)
    {
      auto result = r.result;
      pendingResults[index] = pendingResults[pendingResults.size()-1];
      pendingResults.pop_back();
      return result;
    }
  }
  return FUrbiscriptValue();
}

AActor* AUrbiBridge::spawnActor(UClass* classPtr, FTransform tr, AActor* templ)
{
	FActorSpawnParameters parms;
	parms.Template = templ;
	parms.Name = FName(FString("None"));
	parms.Owner = this;
	return GetWorld()->SpawnActor(classPtr, &tr, parms);
}
APlayerController* AUrbiBridge::getFirstPlayerController()
{
	return GetWorld()->GetFirstPlayerController();
}
#undef LOCTEXT_NAMESPACE


// No longuer needed hack, left for prosperity
#define PER_MODULE_BOILERPLATE_GTFO \
	UE4_VISUALIZERS_HELPERS
#define IMPLEMENT_MODULE_GTFO( ModuleImplClass, ModuleName ) \
		\
		/**/ \
		/* InitializeModule function, called by module manager after this module's DLL has been loaded */ \
		/**/ \
		/* @return	Returns an instance of this module */ \
		/**/ \
		extern "C" DLLEXPORT IModuleInterface* InitializeModule() \
		{ \
			return new ModuleImplClass(); \
		} \
		/* Forced reference to this function is added by the linker to check that each module uses IMPLEMENT_MODULE */ \
		extern "C" void IMPLEMENT_MODULE_##ModuleName() { } \
		PER_MODULE_BOILERPLATE_GTFO \
		PER_MODULE_BOILERPLATE_ANYLINK(ModuleImplClass, ModuleName)


IMPLEMENT_MODULE(FurbiModule, orbi)
//UStart(Unreal)

UWrapper* UWrapper::Instance = nullptr;

// Allocate a callback from our pool, creating new objects as needed
static std::pair<UWrapper*, UFunction*> wrapperRegisterFunction(UFunction* sig)
{
	int index = AUrbiBridge::instance->functionIndex++;
	int instIndex = index / 5;
	int fIndex = (index%5)+1;
	if (AUrbiBridge::instance->wrappers.Num() <= instIndex)
	{
	  auto* uw = NewObject<UWrapper>();
	  uw->index = instIndex;
	  AUrbiBridge::instance->wrappers.Push(uw);
	}
	AUrbiBridge::instance->wrappers[instIndex]->signatures[fIndex-1] = sig;
	std::string name = "Function_" + std::to_string(fIndex);
	auto* cls = UWrapper::Instance->GetClass();
	FName fname = FName(FString(name.length(), name.c_str()));

	//find it
	for (TFieldIterator<UFunction> FunctionIt(cls /* ,EFieldIteratorFlags::ExcludeSuper*/);
	    FunctionIt;
		++FunctionIt)
	{
	  UFunction* Function = *FunctionIt;
		if (str(Function->GetName()) == name)
		{
			return std::make_pair(AUrbiBridge::instance->wrappers[instIndex], Function);
		}
	}
	throw std::runtime_error("function not found: " + name);
}

UWrapper::UWrapper()
{
}

static std::vector<std::pair<std::string, urbi::UValue>> eventsToFire;

// Function call to uwrapper: bound delegate triggering
void UWrapper::ProcessEvent(UFunction* function, void* Params)
{
	int findex = std::atoi(str(function->GetName()).substr(9).c_str())-1;
	auto* sig = signatures[findex];
	auto name = "ev_" + std::to_string(index) + "_" + str(function->GetName());
	int sz = 0;
	auto val = unrealInstance->unmarshallFunc(sig, Params, sz);
	// WE ARE NOT IN SCHEDULER!!! cannot touch UVar from here so be async
	eventsToFire.push_back(std::make_pair(name, val));
}

Unreal::Unreal(std::string const& s)
  : urbi::UObject(s)
  {
    unrealInstance = this;
	UBindVar(Unreal, rootDir);
	rootDir = str(FPaths::ProjectDir());
	UBindFunction(Unreal, add);
	UBindFunction(Unreal, listActors);
	UBindFunction(Unreal, listComponents);
	UBindFunction(Unreal, listFields);
	UBindFunction(Unreal, listProperties);
	UBindFunction(Unreal, listData);
	UBindFunction(Unreal, getFunctionSignature);
	UBindFunction(Unreal, callFunction);
	UBindFunction(Unreal, callFunctionDump);
	UBindFunction(Unreal, getPropertyValue);
	UBindFunction(Unreal, setPropertyValue);
	UBindFunction(Unreal, getActorComponentPtr);
	UBindFunction(Unreal, bindDelegate);
	UBindFunction(Unreal, processPendingEvents);
	UBindFunction(Unreal, findClass);
	UBindFunction(Unreal, instantiate);
	UBindFunction(Unreal, holdPtr);
	UBindFunction(Unreal, registerCallResult);
	UBindFunction(Unreal, bindInput);
	UBindFunction(Unreal, listEnhancedInputs);
	UBindFunction(Unreal, bindEnhancedInput);
	UBindFunction(Unreal, getClass);
	UBindFunction(Unreal, getFunctionReturnType);
	UBindFunction(Unreal, getPropertyType);
	UBindFunction(Unreal, checkParseResult);;
  }

// Helper for classparse step
void Unreal::checkParseResult()
{
	urbi::UVar vr("uobjects", "parseResult");
	auto val = vr.val();
	if (val.type == urbi::DATA_STRING || val.type == urbi::DATA_DICTIONARY)
	{
		parseResult = val;
		urbi::UVar vrs("uobjects", "parseResultString");
		parseResultString = (std::string)vrs.val();
	}
}

// Returns the type of an UObject property
std::string Unreal::getPropertyType(std::string const& optr, std::string const& pname)
{
  auto* o = unbox(optr);
  auto* cls = o->GetClass();
  for (TFieldIterator<FProperty> FunctionIt(cls,  (EFieldIterationFlags)(EFieldIterationFlags::IncludeSuper | EFieldIterationFlags::IncludeInterfaces | EFieldIterationFlags::IncludeDeprecated)/* ,EFieldIteratorFlags::ExcludeSuper*/);
	    FunctionIt; 
		++FunctionIt)
	{
  		FProperty* prop = *FunctionIt;
  		if (str(prop->GetName()) == pname)
  		  return str(prop->GetClass()->GetName());
  }
  return "";
}

// Locate a UClass by name and returns it
std::string Unreal::findClass(std::string const& name)
{
	auto* res = FindObject<UClass>(ANY_PACKAGE, *FString(name.c_str()), false);
	if (res == nullptr)
		throw std::runtime_error("class not found");
	return '$' + std::to_string((uint64)res);
}

// Create a new UObject of given class. Beware of GC!
std::string Unreal::instantiate(std::string const& classPtr)
{
	UClass* ptr = (UClass*)(void*)std::atoll(classPtr.substr(1).c_str());
	auto* res = NewObject<::UObject>(UWrapper::Instance, ptr);
	// careful, will be GC-ed
	return '$' + std::to_string((uint64)res);
}

// holds ptr to block GC forever
std::string Unreal::holdPtr(std::string const& sPtr)
{
	::UObject* ptr = (::UObject*)(void*)std::atoll(sPtr.substr(1).c_str());
	UWrapper::Instance->aholder.Push(ptr);
	UWrapper::Instance->holder = ptr;
	return sPtr;
}

// Process async urbiscript requests coming from the rest of Unreal
void Unreal::processPendingEvents()
{
	for (auto const&s: eventsToFire)
	{
		urbi::UVar var("unrealEvents", s.first);
		var = s.second;
	}
	eventsToFire.clear();
	for (auto const&c : pendingCalls)
	{
	  urbi::UList l;
	  l.push_back(urbi::UValue(c.functionName));
	  l.push_back(urbi::UValue(c.uid));
	  if (c.nativeArgs.type != urbi::DATA_VOID)
	    l.push_back(c.nativeArgs);
	  else
	  {
	    urbi::UList largs;
	    for (auto const& v: c.args)
	    {
	      largs.push_back(v.toUValue());
	    }
	    l.push_back(urbi::UValue(largs));
	  }
	  urbi::UVar var("unrealEvents", "functionCall");
	  var = urbi::UValue(l);
	}
	pendingCalls.clear();
	for (auto const& us : executeUrbiscript)
		urbi::send_command(us);
	executeUrbiscript.clear();
	for (auto const& w : pendingWrites)
	{
		urbi::UVar var("unrealWrites", w.first);
		var = w.second;
	}
	pendingWrites.clear();
}
void Unreal::registerCallResult(int64 uid, urbi::UValue res)
{
  auto fval = FUrbiscriptValue::fromUValue(res);
  PendingResult pr;
  pr.uid = uid;
  pr.result = fval;
  pr.nativeResult = res;
  pendingResults.push_back(pr);
}
void Unreal::wipeCache()
{
  actors.clear();
  actorComponents.clear();
}
void UWrapper::onInput(FString tgtvar)
{
	pendingWrites.push_back(std::make_pair(str(tgtvar), urbi::UValue(1)));
}
void UWrapper::onEnhancedInput(const FInputActionInstance& act, FString tgtvar)
{
	auto v = act.GetValue();
	urbi::UValue res;
	switch (v.GetValueType())
	{
	case EInputActionValueType::Boolean:
		res = (bool)v.Get<bool>() ? 1 : 0;
		break;
	case EInputActionValueType::Axis1D:
		res = (double)v.Get<float>();
		break;
	case EInputActionValueType::Axis2D:
	{
		auto vec = v.Get<FVector2D>();
		urbi::UList l;
		l.push_back(vec[0]);
		l.push_back(vec[1]);
		res = l;
	}
	break;
	case EInputActionValueType::Axis3D:
	{
		auto vec = v.Get<FVector>();
		urbi::UList l;
		l.push_back(vec[0]);
		l.push_back(vec[1]);
		l.push_back(vec[2]);
		res = l;
	}
	break;
	}
	pendingWrites.push_back(std::make_pair(str(tgtvar), res));
}

// Find an input action by name
static const UInputAction* findInputAction(std::string const& name)
{
	if (!AUrbiBridge::instance->mappingContext)
		throw std::runtime_error("mapping not set");
	auto& map = AUrbiBridge::instance->mappingContext->GetMappings();
	for (int i = 0; i < map.Num(); i++)
	{
		auto& e = map[i];
		if (str(e.Action->GetName()) == name)
			return e.Action;
	}
	return nullptr;
}
std::vector<std::string> Unreal::listEnhancedInputs(std::string const& objPtr)
{
	std::vector<std::string> res;
	if (!AUrbiBridge::instance->mappingContext)
		throw std::runtime_error("mapping not set");
	auto& map = AUrbiBridge::instance->mappingContext->GetMappings();
	for (int i = 0; i < map.Num(); i++)
	{
		auto& e = map[i];
		res.push_back(str(e.Action->GetName()));
	}
	return res;
}
std::string Unreal::bindEnhancedInput(std::string const& objPtr, std::string const& name, int what)
{
	auto* action = findInputAction(name);
	if (action == nullptr)
		throw std::runtime_error("no such action");
	static int tgtindex = 0;
	std::string tgtvar = "eaction_" + std::to_string(++tgtindex);
	::UObject* uob = (::UObject*)(void*)(uint64)std::atoll(objPtr.substr(1).c_str());
	auto* actor = Cast<AActor>(uob);
	UEnhancedInputComponent* input = Cast<UEnhancedInputComponent>(actor->InputComponent);
	input->BindAction(action, (ETriggerEvent)what, UWrapper::Instance, &UWrapper::onEnhancedInput, FString(tgtvar.c_str()));
	return tgtvar;
}
std::string Unreal::bindInput(std::string const& objPtr, std::string const& name, int what)
{
	static int tgtindex = 0;
	std::string tgtvar = "action_" + std::to_string(++tgtindex);
	::UObject* uob = (::UObject*)(void*)(uint64)std::atoll(objPtr.substr(1).c_str());
	auto* actor = Cast<AActor>(uob);
	UInputComponent* input = actor->InputComponent;
	//UEnhancedInputComponent* input = Cast<UEnhancedInputComponent>(actor->InputComponent);
	input->BindAction<UWrapper::FInputDelegate>(FName(FString(name.c_str())), (EInputEvent)what, UWrapper::Instance, &UWrapper::onInput, FString(tgtvar.c_str()));
	return tgtvar;
}
std::string Unreal::bindDelegate(std::string const& objPtr, std::string delegateName)
{
  /* Delegate binding is achieved by allocating an unused UWrapper function,
  *  binding it to the delegate using introspection.
  *  Upon trigger UWrapper::processEvent will be called by unreal, which
  *  will trigger an urbiscript write to a variable.
  *  The unreal.u helper code takes care of converting that to an user-friendly
  *  event.
  */
	::UObject* uob = (::UObject*)(void*)(uint64)std::atoll(objPtr.substr(1).c_str());
	FProperty* prop = getProperty(uob, delegateName);
	void* data = (unsigned char*)uob + prop->GetOffset_ForInternal();
	std::string result;
	if (auto* mdp = CastField<FMulticastDelegateProperty>(prop))
	{
		auto instAndFunc = wrapperRegisterFunction(mdp->SignatureFunction);
		FScriptDelegate tsd;
		tsd.BindUFunction(instAndFunc.first, FName(instAndFunc.second->GetName()));
		mdp->AddDelegate(tsd, nullptr, data);
		result = "ev_" + std::to_string(instAndFunc.first->index) + "_" + str(instAndFunc.second->GetName());
	}
	else if (auto* dp = CastField<FDelegateProperty>(prop))
	{
		TScriptDelegate<>* td = (TScriptDelegate<>*)data;
		auto instAndFunc = wrapperRegisterFunction(dp->SignatureFunction);
		td->BindUFunction(instAndFunc.first, FName(instAndFunc.second->GetName()));
		result = "ev_" + std::to_string(instAndFunc.first->index) + "_" + str(instAndFunc.second->GetName());
	}
	else
		throw std::runtime_error("Not a delegate: " + str(prop->GetClass()->GetName()));
	urbi::UVar var("unrealEvents", result);
	var = "unfired";
	return result;
}

// Returns the class of an object
std::string Unreal::getClass(std::string const& objPtr)
{
	return box(unbox(objPtr)->GetClass());
}
std::string Unreal::getActorComponentPtr(std::string const& actor, std::string const& component)
{
	::UObject* target = getUObject(actor, component);
	return '$' + std::to_string((uint64)target);
}

::UObject* Unreal::getUObject(std::string const& actor, std::string const& component)
{
	if (actor == "")
		return world;
	if (component == "")
	{
		if (actor[0] == '$')
			return unbox(actor);
		auto a = getActor(actor);
		if (!a)
			throw std::runtime_error("no such actor");
		return *a;
	}
	auto ac = getActorComponent(actor, component);
	if (!ac)
		throw std::runtime_error("no such actor or component");
	return *ac;
}
UFunction* Unreal::getFunction(::UObject* uo, std::string const& function)
{
	for (TFieldIterator<UFunction> FunctionIt(uo->GetClass() /* ,EFieldIteratorFlags::ExcludeSuper*/);
	    FunctionIt; 
		++FunctionIt)
	{
  		UFunction* Function = *FunctionIt;
		if (str(Function->GetName()) == function)
		  return Function;
	}
	throw std::runtime_error("function not found");
}

FProperty* Unreal::getProperty(::UObject* uo, std::string const& propName)
{
	UClass* kls = uo->GetClass();
	if (auto* uc = Cast<UUrbiComponent>(uo))
	{
		if (uc->classIndex != -1)
		{
			UE_LOG(LogScript, Warning, TEXT("KLS OVERRIDE"));
			kls = gInnerClassPointers[uc->classIndex];
		}
	}
	for (TFieldIterator<FProperty> FunctionIt(kls /* ,EFieldIteratorFlags::ExcludeSuper*/);
	    FunctionIt; 
		++FunctionIt)
	{
  		FProperty* Function = *FunctionIt;
		if (str(Function->GetName()) == propName)
		  return Function;
	}
	throw std::runtime_error("property not found:" + propName);
}

std::string getStructSignature(FProperty* prop);

// Debugging helpers

std::string getPropSignature(FProperty* prop)
{
	std::string res;
	auto ct = str(prop->GetClass()->GetName());
	// + " @ " + str(Param->GetClass()->GetDescription());
	if (ct == "IntProperty")
		res = ("i");
	else if (ct == "BoolProperty")
		res = "b";
	else if (ct == "FloatProperty")
		res = "f";
	else if (ct == "DoubleProperty")
		res += "d";
	else if (ct == "WeakObjectProperty")
		res += "W";
	else if (ct == "ByteProperty")
		res = "B";
	else if (ct == "ObjectProperty")
		res = "o";
	else if (ct == "EnumProperty")
		res = "e";
	else if (ct == "ArrayProperty")
		res = "a";
	else if (ct == "NameProperty")
		res = "n";
	else if (ct == "StrProperty")
		res += "s";
	else if (ct == "ClassProperty")
		res = "C";
	else if (ct == "StructProperty")
		res = getStructSignature(prop);
	else
		res = "UNK(" + ct + ")";
	res += "(" + std::to_string(prop->GetOffset_ForInternal()) + ")";
	return res;
}
std::string getClassSignature(UClass* cls)
{
	std::string res = "{";
	for (TFieldIterator<FProperty> FunctionIt(cls,  (EFieldIterationFlags)(EFieldIterationFlags::IncludeSuper | EFieldIterationFlags::IncludeDeprecated)/* ,EFieldIteratorFlags::ExcludeSuper*/);
	    FunctionIt; 
		++FunctionIt)
	{
  		FProperty* Function = *FunctionIt;
		res += getPropSignature(Function);
	}
	return res + '}';
}
std::string getStructSignature(FProperty* prop)
{
	std::string res = "{";
	for (TFieldIterator<FProperty> ParamIt(((FStructProperty*)prop)->Struct); ParamIt; ++ParamIt)
	{
		res += getPropSignature(*ParamIt);
	}
	return res + "}";
}
std::string Unreal::getFunctionSignature(std::string const& actor, std::string const& component, std::string const& function)
{
	::UObject* uo = getUObject(actor, component);
	UFunction* fn = getFunction(uo, function);
	std::string res;
	for (TFieldIterator<FProperty> ParamIt(fn); ParamIt; ++ParamIt)
	{
		res += getPropSignature(*ParamIt);
	}
	res += " " + std::to_string(fn->ParmsSize);
	return res;
}
static int signatureSize(std::string const& s)
{ // Does not take padding into acount, so probably don't use that...
	static std::unordered_map<unsigned char, int> sigSz = {
		{'i',4}, {'b',1}, {'f', 4}, {'d',8}, {'W', 8}, {'B', 1}, {'o',8}, {'e', 1},
		{'a',16}, {'n', 12}, {'s', 16}, {'C', 8},{'{', 0}, {'}', 0}
	};
	int res = 0;
	for (unsigned char c: s)
		res += sigSz[c];
	return res;
}

std::string Unreal::getFunctionReturnType(std::string const& objPtr, std::string const& funcName)
{
	auto* uo = unbox(objPtr);
	UFunction* func = getFunction(uo, funcName);
	auto* prop = func->GetReturnProperty();
	if (prop == nullptr)
		return "void";
	if (auto* sp = CastField<FStructProperty>(prop))
	{
		return str(sp->Struct->GetName());
	}
	auto ct = str(prop->GetClass()->GetName());
	if (ct == "ObjectProperty")
		return "UObject";
	return "Other"; // we could elaborate more but don't care yet
}
template<int I> struct SizedPad
{
	unsigned char data[I];
};

// Marshals an urbi::UList into a TArray. Huge ABI usumptions here, no choice.
template<int I> void Unreal::marshallArraySized(std::vector<unsigned char>& res, urbi::UList const& data, FProperty* innerProp, Destructors& destructors)
{
	std::vector<unsigned char> inner;
	for (auto iv : data)
	{
		int istart = inner.size();
		std::vector<ExtraOut> extraOut; //NA
		marshallOne(inner, innerProp, *iv, extraOut, -1, destructors);
	}
	int rpos = res.size();
	res.resize(res.size() + 16);
	TArray<SizedPad<I>>* ta = new((void*)&res[rpos]) TArray<SizedPad<I>>();
	ta->SetNum(data.size());
	if (inner.size() != 0)
		memcpy(&(*ta)[0], &inner[0], inner.size());
	destructors.push_back([rpos](std::vector<unsigned char>& v) {
		TArray<SizedPad<I>>* ta = (TArray<SizedPad<I>>*) & v[rpos];
		ta->~TArray();
		});
}

void Unreal::marshallArray(std::vector<unsigned char>& res, urbi::UList const& data, FProperty* innerProp, Destructors& destructors)
{
	int sz = innerProp->GetSize();
#define BOUNCE(s) if (sz == s) { marshallArraySized<s>(res,data,innerProp,destructors); return;}
	BOUNCE(4)
	BOUNCE(8)
	BOUNCE(12)
	BOUNCE(16)
	BOUNCE(20)
	BOUNCE(24)
	BOUNCE(32)
	BOUNCE(40)
	BOUNCE(48)
	BOUNCE(56)
	BOUNCE(64)
	// FIXME ERR
#undef BOUNCE
  UE_LOG(LogScript, Error, TEXT("Marshalling array of unhandled size %d, expect crash"), sz);
}

// Marshall one urbiscript value to type 'prop', appending to 'res'.
void Unreal::marshallOne(std::vector<unsigned char>& res, FProperty* prop, urbi::UValue const& v, std::vector<ExtraOut>& extraOut, int start, Destructors& destructors)
{
	auto ct = str(prop->GetClass()->GetName());
	auto rpos = res.size();
	auto expectedOffset = prop->GetOffset_ForInternal();
	if (start != -1 && rpos-start != expectedOffset)
	{
		res.resize(start + expectedOffset);
		rpos = res.size();
	}
	if (ct == "IntProperty")
	{
		int val = v;
		res.resize(res.size()+4);
		*(int*)&res[rpos] = val;
	}
	else if (ct == "FloatProperty")
	{
		float val = (urbi::ufloat)v;
		res.resize(res.size()+4);
		*(float*)&res[rpos] = val;
	}
	else if (ct == "DoubleProperty")
	{
		double val = v;
		res.resize(res.size()+8);
		*(double*)&res[rpos] = val;
	}
	else if (ct == "BoolProperty" || ct == "ByteProperty" || ct == "EnumProperty")
	{
		unsigned char val = (unsigned int)v;
		res.resize(res.size()+1);
		*(unsigned char*)&res[rpos] = val;
	}
	else if (ct == "NameProperty")
	{
		std::string sval = (std::string)v;
		FName fname(FString(sval.c_str()));
		res.resize(res.size()+12);
		*(FName*)&res[rpos] = fname;
	}
	else if (ct == "StrProperty")
	{
		std::string sval = (std::string)v;
		FString fstr(sval.c_str());
		res.resize(res.size()+16);
		*(FString*)&res[rpos] = fstr;
	}
	else if (ct == "StructProperty")
	{
		if (v.type == urbi::DATA_STRING)
		{
			std::string sv = (std::string)v;
			if (sv[0] == '$')
			{ // Attempt at reference handling, not sure if useful
				void* usptr = (void*)std::atoll(sv.substr(1).c_str());
				UScriptStruct* targetUss = (UScriptStruct*)usptr;
				int sz = targetUss->GetStructureSize();
				void* data = malloc(sz);
				targetUss->InitializeStruct(data);
				if (rpos%8)
				{
					int pad = 8-(rpos%8);
					res.resize(res.size()+pad);
					rpos += pad;
				}
				res.resize(res.size()+8);
				ExtraOut eo;
				eo.data = data;
				eo.offset = rpos;
				eo.type = targetUss;
				*(void**)&res[rpos] = data;
				extraOut.push_back(eo);
			}
			else
			{ // fill with zeros
				auto sprop = CastField<FStructProperty>(prop);
				int sz = sprop->GetSize();
				//std::string sig = getStructSignature(prop);
				//int sz = signatureSize(sig); DOES NOT PAD!!
				res.resize(res.size()+sz);
				if (sv[0] == '@')
				{
					ExtraOut eo;
					eo.data = nullptr;
					eo.offset = rpos;
					eo.type = sprop->Struct;
					extraOut.push_back(eo);
				}
			}
		}
		else
		{
			urbi::UList lst = v;
			int lpos = 0;
			for (TFieldIterator<FProperty> ParamIt(((FStructProperty*)prop)->Struct); ParamIt; ++ParamIt, ++lpos)
			{
				marshallOne(res, *ParamIt, lst[lpos], extraOut, rpos, destructors);
			}
		}
	}
	else if (ct == "ClassProperty")
	{
		auto* ptr = unbox<UClass>(v);
		res.resize(res.size() + 8);
		*(void**)&res[rpos] = ptr;
	}
	else if (ct == "ObjectProperty")
	{
		if (v.type == urbi::DATA_LIST)
		{
			urbi::UList lst = v;
			std::string obj = lst[0];
			std::string comp;
			if (lst.size() > 1)
			{
				auto omp = lst[1];
				comp = (std::string)omp;
			}
			auto* uob = getUObject(obj, comp);
			res.resize(res.size()+8);
			*(void**)&res[rpos] = uob;
		}
		else
		{
			std::string sbptr = v;
			if (sbptr[0] != '$')
				throw std::runtime_error("not a pointer");
			auto sptr = sbptr.substr(1);
			uint64 lptr = std::atoll(sptr.c_str());
			void* ptr = (void*)lptr;
			res.resize(res.size()+8);
			*(void**)&res[rpos] = ptr;
		}
	}
	else if (ct == "ArrayProperty")
	{
		urbi::UList lst = v;
		FArrayProperty* arrProp = CastField<FArrayProperty>(prop);
		FProperty* innerProp = arrProp->Inner;
		if (str(innerProp->GetClass()->GetName()) == "DoubleProperty")
		{
			res.resize(res.size() + 16);
			TArray<double>* ta = new((void*)&res[rpos]) TArray<double>();
			for (auto& lv : lst)
			{
				double lvv = *lv;
				ta->Add((double)lvv);
			}
			// FIXME destruct
		}
		else
		{
			marshallArray(res, lst, innerProp, destructors);
		}
	}
	else
		throw std::runtime_error("unhandled argument type in marshallOne" + ct);
}

// Marshall function arguments
std::vector<unsigned char> Unreal::marshall(UFunction* func, urbi::UList const& args, FProperty* ret, std::vector<ExtraOut>& extraOut, Destructors& destructors)
{
	std::vector<unsigned char> res;
	int p = 0;
	for (TFieldIterator<FProperty> ParamIt(func); ParamIt; ++ParamIt,++p)
	{
		if (ret == *ParamIt)
		{
			// we still need to pad
			auto expectedOffset = (*ParamIt)->GetOffset_ForInternal();
			if (expectedOffset > res.size())
				res.resize(expectedOffset);
			continue;
		}
		marshallOne(res, *ParamIt, args[p], extraOut,0, destructors);
	}
	return res;
}
urbi::UValue Unreal::unmarshallClass(UClass* prop, void* data, int& sz)
{
	urbi::UList l;
	for (TFieldIterator<FProperty> ParamIt(((FStructProperty*)prop)->Struct); ParamIt; ++ParamIt)
	{
		auto v = unmarshall(*ParamIt, data, sz);
		l.push_back(v);
	}
	return urbi::UValue(l);
}

// unmarshall native data 'data' of type 'prop' into an urbiscript value
urbi::UValue Unreal::unmarshall(FProperty* prop, void* data, int& sz)
{
	auto ct = str(prop->GetClass()->GetName());
	if (ct == "IntProperty")
	{
		sz = 4;
		return urbi::UValue(*(int*)data);
	}
	if (ct == "FloatProperty")
	{
		sz = 4;
		return urbi::UValue(*(float*)data);
	}
	if (ct == "DoubleProperty")
	{
		sz = 8;
		return urbi::UValue(*(double*)data);
	}
	if (ct == "BoolProperty" || ct == "ByteProperty" || ct == "EnumProperty")
	{
		sz = 1;
		return urbi::UValue((int)*(unsigned char*)data);
	}
	if (ct == "StrProperty")
	{
		sz = 16;
		FString* fstr = (FString*)data;
		return urbi::UValue(std::string(TCHAR_TO_ANSI(**fstr)));
	}
	if (ct == "NameProperty")
	{
		sz = 12;
		FName* fname = (FName*)data;
		return urbi::UValue(std::string(TCHAR_TO_ANSI(*(fname->ToString()))));
	}
	if (ct == "StructProperty")
	{
		auto* fs = CastField<FStructProperty>(prop);
		if (fs->Struct == FActorInstanceHandle::StaticStruct())
		{
			sz = fs->Struct->GetStructureSize();
			FActorInstanceHandle* h = (FActorInstanceHandle*)data;
			auto* a = h->FetchActor();
			return urbi::UValue('$' + std::to_string((uint64)a));
		}	
		urbi::UList l;
		int p = 0;
		for (TFieldIterator<FProperty> ParamIt(((FStructProperty*)prop)->Struct); ParamIt; ++ParamIt)
		{
			int ssz = 0;
			urbi::UValue v = unmarshall(*ParamIt, ((unsigned char*)data)+(*ParamIt)->GetOffset_ForInternal(), ssz);
			p += ssz;
			l.push_back(v);
		}
		sz = p;
		return urbi::UValue(l);
	}
	if (CastField<FObjectPropertyBase>(prop))
	{
		void* ptr = *(void**)data;
		sz = 8;
		return urbi::UValue('$' + std::to_string((uint64)ptr));
	}
	if (FArrayProperty* aprop = CastField<FArrayProperty>(prop))
	{ // Cue Mc Guyver music!
		auto* pinner = aprop->Inner;
		urbi::UList l;
		sz = 16;
		TArray<unsigned char>* ta = (TArray<unsigned char>*)data;
		int count = ta->Num(); // big assumption here
		unsigned char* idata = &(*ta)[0];
		int ipos = 0;
		for (int i = 0; i < count; i++)
		{
			int isz = 0;
			auto iv = unmarshall(pinner, &idata[ipos], isz);
			l.push_back(iv);
			ipos += isz;
		}
		return urbi::UValue(l);
	}
	throw std::runtime_error("unmanaged prop type in unmarshall: " + ct);
}
urbi::UValue Unreal::unmarshallStruct(UStruct* ustruct, void* data, int& sz)
{
	urbi::UList l;
	int p = 0;
	for (TFieldIterator<FProperty> ParamIt(ustruct); ParamIt; ++ParamIt)
	{
		int ssz = 0;
		urbi::UValue v = unmarshall(*ParamIt, ((unsigned char*)data)+(*ParamIt)->GetOffset_ForInternal(), ssz);
		p += ssz;
		l.push_back(v);
	}
	sz = p;
	return urbi::UValue(l);
}
urbi::UValue Unreal::unmarshallFunc(UFunction* func, void* data, int& sz)
{
	urbi::UList l;
	int p = 0;
	for (TFieldIterator<FProperty> ParamIt(func); ParamIt; ++ParamIt)
	{
		int ssz = 0;
		urbi::UValue v = unmarshall(*ParamIt, ((unsigned char*)data)+(*ParamIt)->GetOffset_ForInternal(), ssz);
		p += ssz;
		l.push_back(v);
	}
	sz = p;
	return urbi::UValue(l);
}
// Call function but dump the whole argument struct out instead of just the return value.
 urbi::UValue Unreal::callFunctionDump(std::string const& actor, std::string const& component, std::string const& function, urbi::UValue const& argvalue)
 {
	::UObject* target = getUObject(actor, component);
	UFunction* func = getFunction(target, function);
	urbi::UList args = argvalue;
	auto* rp = func->GetReturnProperty();
	std::vector<ExtraOut> extraOut;
	Destructors destructors;
	auto argv = marshall(func, args, rp, extraOut, destructors);
	// handle return value
	int rvo = func->ReturnValueOffset;
	auto argsz = argv.size();
	if (rp)
		argv.resize(argv.size()+100); // fixme!
	target->ProcessEvent(func, &argv[0]);
	int sz = 0;
	auto res = unmarshallFunc(func, &argv[0], sz);
	for (auto& d : destructors) d(argv);
	return res;
 }
 urbi::UValue Unreal::callFunction(std::string const& actor, std::string const& component, std::string const& function, urbi::UValue const& argvalue)
 {
	::UObject* target = getUObject(actor, component);
	UFunction* func = getFunction(target, function);
	urbi::UList args = argvalue;
	auto* rp = func->GetReturnProperty();
	std::vector<ExtraOut> extraOut;
	Destructors destructors;
	auto argv = marshall(func, args, rp, extraOut, destructors);
	// handle return value
	int rvo = func->ReturnValueOffset;
	auto argsz = argv.size();
	if (rp)
		argv.resize(argv.size()+100); // fixme!
	target->ProcessEvent(func, &argv[0]);
	urbi::UValue ret = urbi::UValue(0);
	if (rp)
	{
		if (argsz > rvo)
			std::cerr << "return value offset mismatch for call to " << function << " " << rvo << " " << argsz << std::endl;
		int unu = 0;
		ret = unmarshall(rp, &argv[rvo], unu); // trust rvo, padding issues
	}
	if (extraOut.size()!=0)
	{
		urbi::UList l;
		l.push_back(ret);
		for (auto const&eo: extraOut)
		{
			int sz = 0;
			void* data = eo.data;
			if (data == nullptr)
				data = &argv[eo.offset];
			l.push_back(unmarshallStruct(eo.type, data, sz));
		}
		ret = l;
	}
	for (auto& d : destructors) d(argv);
	return ret;
 }

 void Unreal::setPropertyValue(std::string const& actor, std::string const& component, std::string const& propName, urbi::UValue const& val)
 {
	::UObject* target = getUObject(actor, component);
	FProperty* prop = getProperty(target, propName);
	std::vector<unsigned char> res;
	std::vector<ExtraOut> eo;
	Destructors destr;
	marshallOne(res, prop, val, eo, -1, destr);
	void* data = (unsigned char*)target + prop->GetOffset_ForInternal();
	//UE_LOG(LogScript, Warning, TEXT("prop write of %d"), res.size());
	memcpy(data, res.data(), res.size());
 }

 urbi::UValue Unreal::getPropertyValue(std::string const& actor, std::string const& component, std::string const& propName)
 {
	::UObject* target = getUObject(actor, component);
	FProperty* prop = getProperty(target, propName);
	void* data = (unsigned char*)target + prop->GetOffset_ForInternal();
	int sz = 0;
	auto uval = unmarshall(prop, data, sz);
	return uval;
 }
 template<typename T> void listStuffOf(UClass * cls, std::vector<std::string>& res)
 {
	for (TFieldIterator<T> FunctionIt(cls,  (EFieldIterationFlags)(EFieldIterationFlags::IncludeSuper | EFieldIterationFlags::IncludeInterfaces | EFieldIterationFlags::IncludeDeprecated)/* ,EFieldIteratorFlags::ExcludeSuper*/);
	    FunctionIt; 
		++FunctionIt)
	{
  		T* Function = *FunctionIt;
		res.push_back(str(Function->GetName()));
	}
 }
static std::vector<std::string> listFunctionsOf(UClass * cls)
{
	std::vector<std::string> res;
	listStuffOf<UFunction>(cls, res);
	return res;
}
static std::vector<std::string> listPropertiesOf(UClass* cls)
{
	std::vector<std::string> res;
	listStuffOf<FProperty>(cls, res);
	return res;
}
static std::vector<std::string> listDataOf(UClass* cls)
{
	std::vector<std::string> res;
	listStuffOf<FStructProperty>(cls, res);
	listStuffOf<FStrProperty>(cls, res);
	listStuffOf<FBoolProperty>(cls, res);
	listStuffOf<FNumericProperty>(cls, res);
	listStuffOf<FObjectPropertyBase>(cls, res);
	listStuffOf<FArrayProperty>(cls, res);
	return res;
}

std::vector<std::string> Unreal::listFields(std::string const& actor, std::string const& component)
{
	return listFunctionsOf(getUObject(actor, component)->GetClass());
}	
std::vector<std::string> Unreal::listProperties(std::string const& actor, std::string const& component)
{
	return listPropertiesOf(getUObject(actor, component)->GetClass());
}
std::vector<std::string> Unreal::listData(std::string const& actor, std::string const& component)
{
	::UObject* uo = getUObject(actor, component);
	UClass* kls = uo->GetClass();
	if (auto* uc = Cast<UUrbiComponent>(uo))
	{
		if (uc->classIndex != -1)
		{
			UE_LOG(LogScript, Warning, TEXT("KLS OVERRIDE"));
			kls = gInnerClassPointers[uc->classIndex];
		}
	}
	return listDataOf(kls);
}
std::optional<UActorComponent*> Unreal::getActorComponent(std::string const& actor, std::string const& component)
{
	auto k = actor + "$" + component;
	auto it = actorComponents.find(k);
	if (it != actorComponents.end())
		return it->second;
	auto a = getActor(actor);
	if (!a)
		return std::nullopt;
	std::optional<UActorComponent*> res;
	(*a)->ForEachComponent(false, [&](UActorComponent* comp) {
		if (str(comp->GetName()) == component)
		{
			res = comp;
		}
	});
	if (res)
		actorComponents[k] = *res;
	return res;
} 
std::optional<AActor*> Unreal::getActor(std::string const& name)
{
	if (name[0] == '$')
	{
		::UObject* uo = unbox(name);
		auto* a = Cast<AActor>(uo);
		if (a) return a;
		else return std::nullopt;
	}
	auto it = actors.find(name);
	if (it != actors.end())
		return it->second;
	FActorIterator AllActorsItr = FActorIterator(world);
	while (AllActorsItr)
	{
	  auto* a = *AllActorsItr;
	  if (str(a->GetName()) == name)
	  {
		actors[name] = a;
		return a;
	  }
	  ++AllActorsItr;
	}
	return std::nullopt;
}
std::vector<std::string> Unreal::listActors()
{
	std::vector<std::string> res;
	FActorIterator AllActorsItr = FActorIterator(world);
	while (AllActorsItr)
	{
	  auto* a = *AllActorsItr;
	  auto strng = str(a->GetName());
	  res.push_back(urbi::UValue(strng));
	  ++AllActorsItr;
	}
	return res;
}

std::vector<std::string> Unreal::listComponents(std::string const& actor)
{
	auto aptr = getActor(actor);
	if (!aptr)
	    throw std::runtime_error("No such actor");
	std::vector<std::string> res;
	(*aptr)->ForEachComponent(false, [&](UActorComponent* comp) {
		res.push_back(str(comp->GetName()));
	});
	return res;
}
double Unreal::add(double va, double vb)
{
	return va+vb;
}

void UUrbiComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	UObject* ptr = (classIndex == -1) ? (UObject*)GetOwner() : (UObject*)this;
	auto cls = str(className);
	std::string sptr = "$" + std::to_string((int64)ptr);
	executeUrbiscript.push_back("Unreal.UObject.spawn(\"" + cls + "\",\"" + sptr + "\"," + (noAutoKill?"0":"1") + "); ");
}
void UUrbiComponent::EndPlay(EEndPlayReason::Type reason)
{
	UActorComponent::EndPlay(reason);
	auto* ptr = GetOwner();
	auto cls = str(className);
	std::string sptr = "$" + std::to_string((int64)ptr);
	executeUrbiscript.push_back("Unreal.UObject.despawn(\"" + cls + "\",\"" + sptr + "\");");
}

// function execution request from Unreal
static void exec(UObject* Context, FFrame& Stack, void*const  Z_Param__Result, int I, int F)
{
  FProperty* rprop = nullptr;
  UFunction* Function = gFunctions[I][F];
  urbi::UList uargs;
  for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
	  rprop = *ParamIt;
	}
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
	  if (*ParamIt == rprop)
	    break;
	  unsigned char data[256];
	  Stack.StepCompiledIn(data, (*ParamIt)->GetClass());
	  int sz = 0;
	  auto v = unrealInstance->unmarshall(*ParamIt, data, sz);
	  uargs.push_back(v);
	}
	Stack.Code += !!Stack.Code; // P_FINISH
	PendingCall pc;
  pc.uid = ++nextCallUid;
  pc.functionName = "Unreal." + gClassModels[I].name + "_" + box(Context) + "." + str(Function->GetName());
  pc.nativeArgs = uargs;
  pendingCalls.push_back(pc);
  urbi::set_update_tick(true); // FIXME improve (separate processPendingEvents from ticking)
  while (true)
  {
    urbi::step_kernel();
    for (auto const& pr: pendingResults)
    {
      if (pr.uid == pc.uid)
      {
        std::vector<unsigned char> data;
        std::vector<ExtraOut> eo;
        Destructors ds;
        unrealInstance->marshallOne(data, rprop, pr.nativeResult, eo, -1, ds);
        memcpy(Z_Param__Result, data.data(), data.size());
        return;
      }
    }
  }
}

template<int I, int F>
void UUrbiComponent::execfunction(UObject* Context, FFrame& Stack, void*const  Z_Param__Result)
{
  exec(Context, Stack, Z_Param__Result, I, F);
}

void UUrbiComponent::ProcessEvent(UFunction* Function, void* Parms)
{ // this is not called, rats, hence the execfunction
  FProperty* rprop = nullptr;
  for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
	  rprop = *ParamIt;
	}
  int sz = 0;
  urbi::UValue args = unrealInstance->unmarshallFunc(Function, Parms, sz);
  PendingCall pc;
  pc.uid = ++nextCallUid;
  pc.functionName = "uobjects.Unreal." + gClassModels[classIndex].name + "_" + box(this) + "." + str(Function->GetName());
  pc.nativeArgs = args;
  pendingCalls.push_back(pc);
  urbi::set_update_tick(true); // FIXME improve
  while (true)
  {
    urbi::step_kernel();
    for (auto const& pr: pendingResults)
    {
      if (pr.uid == pc.uid)
      {
        std::vector<unsigned char> data;
        std::vector<ExtraOut> eo;
        Destructors ds;
        unrealInstance->marshallOne(data, rprop, pr.nativeResult, eo, -1, ds);
        memcpy(((unsigned char*)Parms)+rprop->GetOffset_ForInternal(), data.data(), data.size());
        return;
      }
    }
  }
}
UStart(Unreal);