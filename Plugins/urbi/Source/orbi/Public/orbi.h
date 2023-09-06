
#pragma once
#define _USE_MATH_DEFINES 1
//#define M_PI 3.14159265
#define BOOST_DISABLE_ABI_HEADERS 1
//#define BOOST_ALL_DYN_LINK 1

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Actor.h"
#include <EnhancedInputComponent.h>
#include "InputMappingContext.h"

#include "orbi.generated.h"

class FurbiModule : public IModuleInterface
{
public:
    UFUNCTION()
	void sampleFunctionOne();
	UFUNCTION()
	void sampleFunctionTwo(int i);
	UFUNCTION()
	void onFrameTimer();
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
#if WITH_EDITOR
	void AddMenu(FMenuBarBuilder& MenuBuilder);
	void FillMenu(FMenuBuilder& MenuBuilder);
#endif
	virtual void ShutdownModule() override;
private:
	FTimerHandle timerHandle;
};

struct FUrbiscriptValue;

namespace urbi
{
  class UValue;
}

// Unreal pendant of urbi::UValue used for generic urbi calls
USTRUCT(BlueprintType)
struct FUrbiscriptValue
{
   GENERATED_BODY()
   UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Misc)
   int64 longValue;
   
   UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Misc)
   FString stringValue;
   
   UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Misc)
   double doubleValue;

   //loops are not alowedàUPROPERTY(EditAnywhere,BlueprintReadWrite)
   TArray<FUrbiscriptValue> listValue;

   static const constexpr int64 UNSET_LONG = 0xDBC9B729DBC9B729;
   static const constexpr double UNSET_DOUBLE = 1.812684984116541984e125;
   static const constexpr char* UNSET_STRING = "<<urbiscript unset>>";
   FUrbiscriptValue()
   {
     longValue = UNSET_LONG;
     stringValue = UNSET_STRING;
     doubleValue = UNSET_DOUBLE;
   }

   urbi::UValue toUValue() const;
   static FUrbiscriptValue fromUValue(urbi::UValue const& uval);
};

// Internal helper for binding delegates and other misc stuff
UCLASS()
class UWrapper: public UObject
{
	GENERATED_BODY()
	public:
	  int index = 0;
		UWrapper();
		virtual void ProcessEvent( UFunction* Function, void* Parms ) override;
		void onInput(FString tgt);
		void onAxisInput(float v, FString tgt);
		void onEnhancedInput(const FInputActionInstance& act, FString tgt);
		UFUNCTION(BlueprintCallable,Category=Misc)
		void Function_1() {}
		UFUNCTION(BlueprintCallable,Category=Misc)
		void Function_2() {}
		UFUNCTION(BlueprintCallable,Category=Misc)
		void Function_3() {}
		UFUNCTION(BlueprintCallable,Category=Misc)
		void Function_4() {}
		UFUNCTION(BlueprintCallable,Category=Misc)
		void Function_5() {}
		UFunction* signatures[5] = {};
		UPROPERTY()
		UObject* holder;
		UPROPERTY()
		TArray<UObject*> aholder;
	static UWrapper* Instance;
	DECLARE_DELEGATE_OneParam(FInputDelegate, FString);
	//DECLARE_DELEGATE_TwoParams(FInputDelegate, const FInputActionInstance&, FString);
};

// Main urbiscript bridge
UCLASS()
class AUrbiBridge: public AActor
{
	GENERATED_BODY()
public:
	AUrbiBridge();
	int functionIndex = 0;
protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type reason) override;

	// Generic Urbiscript call from blueprints or C++
	UFUNCTION(BlueprintCallable,Category=Misc)
  int64 callUrbi(FString functionName, TArray<FUrbiscriptValue> const& args);
	UFUNCTION(BlueprintCallable,Category=Misc)
	bool hasCallResult(int64 handle);
	UFUNCTION(BlueprintCallable,Category=Misc)
	FUrbiscriptValue getCallResult(int64 handle);

	// UrbiscriptValue helpers
	UFUNCTION(BlueprintCallable,Category=Misc)
	static FUrbiscriptValue addToList(FUrbiscriptValue lst, FUrbiscriptValue value);
	UFUNCTION(BlueprintPure,Category=Misc)
	static FUrbiscriptValue makeListValue(TArray<FUrbiscriptValue> vals);
  UFUNCTION(BlueprintPure,Category=Misc)
	static FUrbiscriptValue makeLongValue(int64 val);
  UFUNCTION(BlueprintPure,Category=Misc)
	static FUrbiscriptValue makeFloatValue(double val);
  UFUNCTION(BlueprintPure,Category=Misc)
	static FUrbiscriptValue makeStringValue(FString val);
	UFUNCTION(BlueprintCallable,Category=Misc)
	static TArray<FUrbiscriptValue> getValueFieldList(FUrbiscriptValue val);

	// Helpers not accessible through introspection
	UFUNCTION(BlueprintCallable,Category=Misc)
	AActor* spawnActor(UClass* classPtr, FTransform tr, AActor* templ);
	UFUNCTION(BlueprintCallable,Category=Misc)
	APlayerController* getFirstPlayerController();
	UPROPERTY(EditAnywhere,Category=Misc)

	bool enabled = false;

	// Configuration
	UPROPERTY(EditAnywhere,Category=Misc)
	int64 ListenPort = 54000;
	UPROPERTY(EditAnywhere,Category=Misc)
	uint64 BudgetMicroseconds = 5000;
	UPROPERTY(EditAnywhere,Category=Misc)
	uint64 BudgetTicks = 100;
private:
	bool loaded = false;
public:
	static AUrbiBridge* instance;
	UPROPERTY(EditAnywhere,Category=Misc)
	UInputMappingContext* mappingContext;
	UPROPERTY()
	TArray<UWrapper*> wrappers;
};

UCLASS(ClassGroup = Misc, meta = (BlueprintSpawnableComponent))
class UUrbiComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,Category=Misc)
	bool noAutoKill = false;
	UPROPERTY(EditAnywhere,Category=Misc)
	FString className;
	int classIndex = -1;
	unsigned char buffer[512];
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type reason) override;
	virtual void ProcessEvent( UFunction* Function, void* Parms ) override;
	template<int I, int F> static void execfunction( UObject* Context, FFrame& Stack, void*const  Z_Param__Result);
};