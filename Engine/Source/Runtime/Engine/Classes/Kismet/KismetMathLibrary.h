// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetMathLibrary.generated.h"

/** Provides different easing functions that can be used in blueprints */
UENUM(BlueprintType)
namespace EEasingFunc
{
	enum Type
	{
		/** Simple linear interpolation. */
		Linear,

		/** Simple step interpolation. */
		Step,

		/** Sinusoidal in interpolation. */
		SinusoidalIn,

		/** Sinusoidal out interpolation. */
		SinusoidalOut,

		/** Sinusoidal in/out interpolation. */
		SinusoidalInOut,

		/** Immediately accelerates, but smoothly decelerates into the target.  Ease amount controlled by BlendExp. */
		EaseIn,

		/** Smoothly accelerates, but does not decelerate into the target.  Ease amount controlled by BlendExp. */
		EaseOut,

		/** Smoothly accelerates and decelerates.  Ease amount controlled by BlendExp. */
		EaseInOut,

		/** Easing in using an exponential */
		ExpoIn,

		/** Easing out using an exponential */
		ExpoOut,

		/** Easing in/out using an exponential method */
		ExpoInOut,

		/** Easing is based on a half circle. */
		CircularIn,

		/** Easing is based on an inverted half circle. */
		CircularOut,

		/** Easing is based on two half circles. */
		CircularInOut,

	};
}

UCLASS(MinimalAPI)
class UKismetMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//
	// Boolean functions.
	//
	
	/* Returns a uniformly distributed random bool*/
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static bool RandomBool();

	/* Returns the logical complement of the Boolean value (NOT A) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NOT Boolean", CompactNodeTitle = "NOT", Keywords = "! not"), Category="Math|Boolean")
	static bool Not_PreBool(bool A);

	/* Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal Boolean", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Boolean")
	static bool EqualEqual_BoolBool(bool A, bool B);

	/* Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual Boolean", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Boolean")
	static bool NotEqual_BoolBool(bool A, bool B);

	/* Returns the logical AND of two values (A AND B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "AND Boolean", CompactNodeTitle = "AND", Keywords = "& and", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanAND(bool A, bool B);

	/* Returns the logical OR of two values (A OR B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "OR Boolean", CompactNodeTitle = "OR", Keywords = "| or", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanOR(bool A, bool B);
		
	/* Returns the logical eXclusive OR of two values (A XOR B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "XOR Boolean", CompactNodeTitle = "XOR", Keywords = "^ xor"), Category="Math|Boolean")
	static bool BooleanXOR(bool A, bool B);

	//
	// Byte functions.
	//

	/* Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte * Byte", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Byte")
	static uint8 Multiply_ByteByte(uint8 A, uint8 B);

	/* Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte / Byte", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Byte")
	static uint8 Divide_ByteByte(uint8 A, uint8 B);

	/* Modulo (A % B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "% (Byte)", CompactNodeTitle = "%", Keywords = "% modulus"), Category="Math|Byte")
	static uint8 Percent_ByteByte(uint8 A, uint8 B);

	/* Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte + Byte", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Byte")
	static uint8 Add_ByteByte(uint8 A, uint8 B);

	/* Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte - Byte", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Byte")
	static uint8 Subtract_ByteByte(uint8 A, uint8 B);

	/* Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte < Byte", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Byte")
	static bool Less_ByteByte(uint8 A, uint8 B);

	/* Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte > Byte", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Byte")
	static bool Greater_ByteByte(uint8 A, uint8 B);

	/* Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte <= Byte", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Byte")
	static bool LessEqual_ByteByte(uint8 A, uint8 B);

	/* Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Byte >= Byte", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Byte")
	static bool GreaterEqual_ByteByte(uint8 A, uint8 B);

	/* Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (Byte)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Byte")
	static bool EqualEqual_ByteByte(uint8 A, uint8 B);

	/* Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual (Byte)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Byte")
	static bool NotEqual_ByteByte(uint8 A, uint8 B);

	//
	// Integer functions.
	//

	/* Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer * integer", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Multiply_IntInt(int32 A, int32 B);

	/* Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer / integer", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Integer")
	static int32 Divide_IntInt(int32 A, int32 B);

	/* Modulo (A % B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "% (integer)", CompactNodeTitle = "%", Keywords = "% modulus"), Category="Math|Integer")
	static int32 Percent_IntInt(int32 A, int32 B);

	/* Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer + integer", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Add_IntInt(int32 A, int32 B);

	/* Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer - integer", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Integer")
	static int32 Subtract_IntInt(int32 A, int32 B);

	/* Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer < integer", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Integer")
	static bool Less_IntInt(int32 A, int32 B);

	/* Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer > integer", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Integer")
	static bool Greater_IntInt(int32 A, int32 B);

	/* Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer <= integer", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Integer")
	static bool LessEqual_IntInt(int32 A, int32 B);

	/* Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "integer >= integer", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Integer")
	static bool GreaterEqual_IntInt(int32 A, int32 B);

	/* Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (integer)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Integer")
	static bool EqualEqual_IntInt(int32 A, int32 B);

	/* Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual (integer)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Integer")
	static bool NotEqual_IntInt(int32 A, int32 B);

	/* Bitwise AND (A & B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Bitwise AND", CompactNodeTitle = "&", Keywords = "& and", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 And_IntInt(int32 A, int32 B);

	/* Bitwise XOR (A ^ B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Bitwise XOR", CompactNodeTitle = "^", Keywords = "^ xor"), Category="Math|Integer")
	static int32 Xor_IntInt(int32 A, int32 B);

	/* Bitwise OR (A | B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Bitwise OR", CompactNodeTitle = "|", Keywords = "| or", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Or_IntInt(int32 A, int32 B);

	/* Sign (integer, returns -1 if A < 0, 0 if A is zero, and +1 if A > 0) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Sign (integer)"), Category="Math|Integer")
	static int32 SignOfInteger(int32 A);

	/* Returns a uniformly distributed random number between 0 and Max - 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomInteger(int32 Max);

	/** Return a random integer between Min and Max (>= Min and <= Max) */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomIntegerInRange(int32 Min, int32 Max);

	/* Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Min (int)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Min(int32 A, int32 B);

	/* Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Max (int)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Max(int32 A, int32 B);

	/* Returns Value clamped to be between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Clamp (int)"), Category="Math|Integer")
	static int32 Clamp(int32 Value, int32 Min, int32 Max);

	/* Returns the absolute (positive) value of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Absolute (int)"), Category="Math|Integer")
	static int32 Abs_Int(int32 A);


	//
	// Float functions.
	//
	/* Power (Base to the Exp-th power) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Power" ), Category="Math|Float")
	static float MultiplyMultiply_FloatFloat(float Base, float Exp);

	/* Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float * float", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float Multiply_FloatFloat(float A, float B);

	/* Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "int * float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Float")
	static float Multiply_IntFloat(int32 A, float B);

	/* Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float / float", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Float")
	static float Divide_FloatFloat(float A, float B);

	/* Modulo (A % B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "% (float)", CompactNodeTitle = "%", Keywords = "% modulus"), Category="Math|Float")
	static float Percent_FloatFloat(float A, float B);

	/** Returns the fractional part of a float. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Fraction(float A);

	/* Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float + float", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float Add_FloatFloat(float A, float B);

	/* Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float - float", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Float")
	static float Subtract_FloatFloat(float A, float B);

	/*Returns true if A is Less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float < float", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Float")
	static bool Less_FloatFloat(float A, float B);

	/*Returns true if A is Greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float > float", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Float")
	static bool Greater_FloatFloat(float A, float B);

	/* Returns true if A is Less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float <= float", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Float")
	static bool LessEqual_FloatFloat(float A, float B);

	/* Returns true if A is Greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "float >= float", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Float")
	static bool GreaterEqual_FloatFloat(float A, float B);

	/* Returns true if A is exactly equal to B (A == B)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (float)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Float")
	static bool EqualEqual_FloatFloat(float A, float B);

	/* Returns true if A is nearly equal to B (|A - B| < ErrorTolerance) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Nearly Equal (float)", Keywords = "== equal"), Category="Math|Float")
	static bool NearlyEqual_FloatFloat(float A, float B, float ErrorTolerance = 1.e-6f);

	/* Returns true if A does not equal B (A != B)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual (float)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Float")
	static bool NotEqual_FloatFloat(float A, float B);

	/* Returns V clamped to be between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "InRange (float)", Min="0.0", Max="1.0"), Category="Math|Float")
	static bool InRange_FloatFloat(float Value, float Min, float Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/* Returns the absolute (positive) value of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Absolute (float)", CompactNodeTitle = "ABS"), Category="Math|Float")
	static float Abs(float A);

	/* Returns the sine of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Sin (Radians)", CompactNodeTitle = "SIN", Keywords = "sine"), Category="Math|Trig")
	static float Sin(float A);

	/* Returns the inverse sine (arcsin) of A (result is in Radians) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Asin (Radians)", CompactNodeTitle = "ASIN", Keywords = "sine"), Category="Math|Trig")
	static float Asin(float A);

	/* Returns the cosine of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Cos (Radians)", CompactNodeTitle = "COS"), Category="Math|Trig")
	static float Cos(float A);

	/* Returns the inverse cosine (arccos) of A (result is in Radians) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Acos (Radians)", CompactNodeTitle = "ACOS"), Category="Math|Trig")
	static float Acos(float A);

	/* Returns the tan of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Tan (Radians)", CompactNodeTitle = "TAN"), Category="Math|Trig")
	static float Tan(float A);

	/* Returns the inverse tan (atan) (result is in Radians)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Atan (Radians)"), Category="Math|Trig")
	static float Atan(float A);

	/* Returns the inverse tan (atan2) of A/B (result is in Radians)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Atan2 (Radians)"), Category="Math|Trig")
	static float Atan2(float A, float B);

	/* Returns exponential(e) to the power A (e^A)*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(CompactNodeTitle = "e"))
	static float Exp(float A);

	/* Returns natural log of A (if e^R == A, returns R)*/
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Loge(float A);

	/* Returns square root of A*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(Keywords = "square root", CompactNodeTitle = "SQRT"))
	static float Sqrt(float A);

	/* Returns square of A (A*A)*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(CompactNodeTitle = "^2"))
	static float Square(float A);

	/** Returns a random float between 0 and 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloat();

	/** Generate a random number between Min and Max */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloatInRange(float Min, float Max);

	/* Returns the value of PI */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Get PI", CompactNodeTitle = "PI"), Category="Math|Trig")
	static float GetPI();

	/* Returns radians value based on the input degrees */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Degrees To Radians", CompactNodeTitle = "D2R"), Category="Math|Trig")
	static float DegreesToRadians(float A);

	/* Returns degrees value based on the input radians */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Radians To Degrees", CompactNodeTitle = "R2D"), Category="Math|Trig")
	static float RadiansToDegrees(float A);

	/* Returns the sin of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Sin (Degrees)", CompactNodeTitle = "SINd"), Category="Math|Trig")
	static float DegSin(float A);

	/* Returns the inverse sin (arcsin) of A (result is in Degrees) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Asin (Degrees)", CompactNodeTitle = "ASINd"), Category="Math|Trig")
	static float DegAsin(float A);

	/* Returns the cos of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Cos (Degrees)", CompactNodeTitle = "COSd"), Category="Math|Trig")
	static float DegCos(float A);

	/* Returns the inverse cos (arccos) of A (result is in Degrees) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Acos (Degrees)", CompactNodeTitle = "ACOSd"), Category="Math|Trig")
	static float DegAcos(float A);

	/* Returns the tan of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Tan (Degrees)", CompactNodeTitle = "TANd"), Category="Math|Trig")
	static float DegTan(float A);

	/* Returns the inverse tan (atan) (result is in Degrees)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Atan (Degrees)"), Category="Math|Trig")
	static float DegAtan(float A);

	/* Returns the inverse tan (atan2) of A/B (result is in Degrees)*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Atan2 (Degrees)"), Category="Math|Trig")
	static float DegAtan2(float A, float B);

	/** 
	 * Clamps an arbitrary angle to be between the given angles.  Will clamp to nearest boundary.
	 * 
	 * @param MinAngleDegrees	"from" angle that defines the beginning of the range of valid angles (sweeping clockwise)
	 * @param MaxAngleDegrees	"to" angle that defines the end of the range of valid angles
	 * @return Returns clamped angle in the range -180..180.
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Clamp Angle"), Category="Math|Float")
	static float ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees);

	/* Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Min (float)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float FMin(float A, float B);

	/* Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Max (float)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float FMax(float A, float B);

	/* Returns V clamped to be between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Clamp (float)", Min="0.0", Max="1.0"), Category="Math|Float")
	static float FClamp(float Value, float Min, float Max);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static void MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static void MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static void MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static void MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Byte")
	static void MaxOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMaxValue, uint8& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Byte")
	static void MinOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMinValue, uint8& MinValue);

	/* Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Lerp(float A, float B, float Alpha);

	/* Easeing  between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (FriendlyName = "Ease", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static float Ease(float A, float B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/* Rounds A to the nearest integer */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static int32 Round(float A);

	/* Rounds A to the largest previous integer */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Floor"), Category="Math|Float")
	static int32 FFloor(float A);

	/* Rounds A to the smallest following integer */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static int32 FCeil(float A);

	/* Returns the number of times Divisor will go into Dividend (i.e., Dividend divided by Divisor), as well as the remainder */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Division (whole and remainder)"), Category="Math|Float")
	static int32 FMod(float Dividend, float Divisor, float& Remainder);

	/* Sign (float, returns -1 if A < 0, 0 if A is zero, and +1 if A > 0) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Sign (float)"), Category="Math|Float")
	static float SignOfFloat(float A);

	/** Returns Value normalized to the given range.  (e.g. 20 normalized to the range 10->50 would result in 0.25) */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float NormalizeToRange(float Value, float RangeMin, float RangeMax);

	/** Returns Value mapped from one range into another.  (e.g. 20 normalized from the range 10->50 to 20->40 would result in 25) */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float MapRange(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB);

	/** Multiplies the input value by pi. */
	UFUNCTION(BlueprintPure, meta=(Keywords = "* multiply"), Category="Math|Float")
	static float MultiplyByPi(float Value);

	/** Interpolate between A and B, applying an ease in/out function.  Exp controls the degree of the curve. */
	UFUNCTION(BlueprintPure, Category = "Math|Float")
	static float FInterpEaseInOut(float A, float B, float Alpha, float Exponent);


	//
	// Vector functions.
	//

	/* Scales Vector A by B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector * float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Vector")
	static FVector Multiply_VectorFloat(FVector A, float B);

	/* Element-wise Vector multiplication (Result = {A.x*B.x, A.y*B.y, A.z*B.z}) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector * vector", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector")
	static FVector Multiply_VectorVector(FVector A, FVector B);

	/* Vector divide by a float */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector / float", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorFloat(FVector A, float B);

	/* Vector divide by vector */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector / vector", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorVector(FVector A, FVector B);

	/* Vector addition */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector + vector", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector")
	static FVector Add_VectorVector(FVector A, FVector B);

	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector + float", CompactNodeTitle = "+", Keywords = "+ add plus"), Category="Math|Vector")
	static FVector Add_VectorFloat(FVector A, float B);

	/* Vector subtraction */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector - vector", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorVector(FVector A, FVector B);

	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector - float", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorFloat(FVector A, float B);

	/* Returns result of vector A rotated by the inverse of Rotator B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "UnrotateVector"), Category="Math|Vector")
	static FVector LessLess_VectorRotator(FVector A, FRotator B);

	/* Returns result of vector A rotated by Rotator B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "RotateVector"), Category="Math|Vector")
	static FVector GreaterGreater_VectorRotator(FVector A, FRotator B);

	/* Returns result of vector A rotated by AngleDeg around Axis */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "RotateVectorAroundAxis"), Category="Math|Vector")
	static FVector RotateAngleAxis(FVector InVect, float AngleDeg, FVector Axis);

	/* Returns true if vector A is equal to vector B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (vector)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Vector")
	static bool EqualEqual_VectorVector(FVector A, FVector B, float ErrorTolerance = 1.e-4f);

	/* Returns true if vector A is not equal to vector B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Not Equal (vector)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Vector")
	static bool NotEqual_VectorVector(FVector A, FVector B, float ErrorTolerance = 1.e-4f);

	/* Returns the dot product of two 3d vectors */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Dot Product", CompactNodeTitle = "."), Category="Math|Vector" )
	static float Dot_VectorVector(FVector A, FVector B);

	/* Returns the cross product of two 3d vectors */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Cross Product", CompactNodeTitle = ""), Category="Math|Vector" )
	static FVector Cross_VectorVector(FVector A, FVector B);

	/* Returns the dot product of two 2d vectors */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Dot Product (2D)", CompactNodeTitle="."), Category="Math|Vector")
	static float DotProduct2D(FVector2D A, FVector2D B);

	/* Returns the cross product of two 2d vectors */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Cross Product (2D)", CompactNodeTitle="x"), Category="Math|Vector")
	static float CrossProduct2D(FVector2D A, FVector2D B);

	/* Returns the length of the FVector */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "VectorLength"), Category="Math|Vector")
	static float VSize(FVector A);

	/** Returns the length of a 2d FVector. */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Vector2dLength"), Category="Math|Vector2D")
	static float VSize2D(FVector2D A);

	/* Returns the squared length of the FVector */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "VectorLengthSquared"), Category="Math|Vector")
	static float VSizeSquared(FVector A);

	/** Returns the squared length of a 2d FVector. */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Vector2dLengthSquared"), Category="Math|Vector2D")
	static float VSize2DSquared(FVector2D A);

	/* Returns a unit normal version of the FVector A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Normalize"), Category="Math|Vector")
	static FVector Normal(FVector A);

	/* Returns a unit normal version of the vector2d A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Normalize2D"), Category="Math|Vector2D")
	static FVector2D Normal2D(FVector2D A);

	/* Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Lerp (vector)"), Category="Math|Vector")
	static FVector VLerp(FVector A, FVector B, float Alpha);

	/* Easeing  between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (FriendlyName = "Ease (vector)", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static FVector VEase(FVector A, FVector B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/* Returns a random vector with length of 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static FVector RandomUnitVector();

	/** Returns a random point within the specified bounding box */
	UFUNCTION(BlueprintPure, Category = "Math|Random")
	static FVector RandomPointInBoundingBox(const FVector& Origin, const FVector& BoxExtent);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution. 
	 * @param ConeDir	The base "center" direction of the cone.
	 * @param ConeHalfAngle		The half-angle of the cone (from ConeDir to edge), in radians.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Random")
	static FVector RandomUnitVectorInCone(FVector ConeDir, float ConeHalfAngle);

	// Mirrors a vector by a normal
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector MirrorVectorByNormal(FVector InVect, FVector InNormal);

	/**
	* Projects one vector (V) onto another (Target) and returns the projected vector.
	* If Target is nearly zero in length, returns the zero vector.
	*
	* @param  V Vector to project.
	* @param  Target Vector on which we are projecting.
	* @return V projected on to Target.
	*/
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords = "ProjectOnTo"))
	static FVector ProjectVectorOnToVector(FVector V, FVector Target);

	 /**
	 * Projects a point onto a plane defined by a point on the plane and a plane normal.
	 *
	 * @param  Point Point to project onto the plane.
	 * @param  PlaneBase A point on the plane.
	 * @param  PlaneNormal Normal of the plane.
	 * @return Point projected onto the plane.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Vector", meta=(Keywords = "ProjectOnTo"))
	static FVector ProjectPointOnToPlane(FVector Point, FVector PlaneBase, FVector PlaneNormal);

	/**
	* Projects a vector onto a plane defined by a normalized vector (PlaneNormal).
	*
	* @param  V Vector to project onto the plane.
	* @param  PlaneNormal Normal of the plane.
	* @return Vector projected onto the plane.
	*/
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords = "ProjectOnTo"))
	static FVector ProjectVectorOnToPlane(FVector V, FVector PlaneNormal);

	/** Negate a vector. */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector NegateVector(FVector A);

	/** Clamp the vector size between a min and max length */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector ClampVectorSize(FVector A, float Min, float Max);

	/** Find the minimum element (X, Y or Z) of a vector */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static float GetMinElement(FVector A);

	/** Find the maximum element (X, Y or Z) of a vector */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static float GetMaxElement(FVector A);

	/** Find the average of an array of vectors */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector GetVectorArrayAverage(const TArray<FVector>& Vectors);

	/** Find the unit direction vector from one position to another. */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector GetDirectionVector(FVector From, FVector To);

	//
	// Rotator functions.
	//

	//Returns true if rotator A is equal to rotator B (A == B) within a specified error tolerance
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (Rotator)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Rotator")
	static bool EqualEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance = 1.e-4f);

	//Returns true if rotator A is not equal to rotator B (A != B) within a specified error tolerance
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Not Equal (Rotator)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Rotator")
	static bool NotEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance = 1.e-4f);

	//Returns rotator representing rotator A scaled by B 
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ScaleRotator", CompactNodeTitle = "*", Keywords = "* multiply rotate rotation"), Category="Math|Rotator")
	static FRotator Multiply_RotatorFloat(FRotator A, float B);

	/** Combine 2 rotations to give you the resulting rotation */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "CombineRotators", Keywords="rotate rotation add"), Category="Math|Rotator")
	static FRotator ComposeRotators(FRotator A, FRotator B);

	/** Negate a rotator*/
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotate rotation"))
	static FRotator NegateRotator(FRotator A);

	/** Get the reference frame direction vectors (axes) described by this rotation */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotate rotation"))
	static void GetAxes(FRotator A, FVector& X, FVector& Y, FVector& Z);

	/** Generates a random rotation, with optional random roll. */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(Keywords="rotate rotation"))
	static FRotator RandomRotator(bool bRoll = false);

	/* Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Lerp (Rotator)"), Category="Math|Rotator")
	static FRotator RLerp(FRotator A, FRotator B, float Alpha, bool bShortestPath);

	/* Easeing  between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (FriendlyName = "Ease (Rotator)", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static FRotator REase(FRotator A, FRotator B, float Alpha, bool bShortestPath, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Normalized A-B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Delta (Rotator)"), Category="Math|Rotator")
	static FRotator NormalizedDeltaRotator(FRotator A, FRotator B);

	/** Create a rotation from an axis and and angle (in degrees) */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="make construct build rotate rotation"))
	static FRotator RotatorFromAxisAndAngle(FVector Axis, float Angle);

	//
	//	LinearColor functions
	//

	/* Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Lerp (LinearColor)"), Category="Math|Color")
	static FLinearColor LinearColorLerp(FLinearColor A, FLinearColor B, float Alpha);

	/* Element-wise multiplication of two linear colors (R*R, G*G, B*B, A*A) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "LinearColor * (LinearColor)", CompactNodeTitle = "*"), Category="Math|Color")
	static FLinearColor Multiply_LinearColorLinearColor(FLinearColor A, FLinearColor B);

	/* Element-wise multiplication of a linear color by a float (F*R, F*G, F*B, F*A) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "LinearColor * Float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Color")
	static FLinearColor Multiply_LinearColorFloat(FLinearColor A, float B);

	//
	// DateTime functions.
	//

	/* Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DateTime + Timespan", CompactNodeTitle="+", Keywords="+ add plus"), Category="Math|DateTime")
	static FDateTime Add_DateTimeTimespan( FDateTime A, FTimespan B );

	/* Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DateTime - Timespan", CompactNodeTitle="-", Keywords="- subtract minus"), Category="Math|DateTime")
	static FDateTime Subtract_DateTimeTimespan( FDateTime A, FTimespan B );

	/* Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Equal (DateTime)", CompactNodeTitle="==", Keywords="== equal"), Category="Math|DateTime")
	static bool EqualEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/* Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="NotEqual (DateTime)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="Math|DateTime")
	static bool NotEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/* Returns true if A is Greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DateTime > DateTime", CompactNodeTitle=">", Keywords="> greater"), Category="Math|DateTime")
	static bool Greater_DateTimeDateTime( FDateTime A, FDateTime B );

	/* Returns true if A is Greater than B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DateTime >= DateTime", CompactNodeTitle=">=", Keywords=">= greater"), Category="Math|DateTime")
	static bool GreaterEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/* Returns true if A is Greater than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DateTime < DateTime", CompactNodeTitle="<", Keywords="< less"), Category="Math|DateTime")
	static bool Less_DateTimeDateTime( FDateTime A, FDateTime B );

	/* Returns true if A is Greater than B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DateTime <= DateTime", CompactNodeTitle="<=", Keywords="<= less"), Category="Math|DateTime")
	static bool LessEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/* Returns the date component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetDate"), Category="Math|DateTime")
	static FDateTime GetDate( FDateTime A );

	/* Returns the day component of A (1 to 31) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetDay"), Category="Math|DateTime")
	static int32 GetDay( FDateTime A );

	/* Returns the day of year of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetDayOfYear"), Category="Math|DateTime")
	static int32 GetDayOfYear( FDateTime A );

	/* Returns the hour component of A (24h format) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetHour"), Category="Math|DateTime")
	static int32 GetHour( FDateTime A );

	/* Returns the hour component of A (12h format) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetHour12"), Category="Math|DateTime")
	static int32 GetHour12( FDateTime A );

	/* Returns the millisecond component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetMillisecond"), Category="Math|DateTime")
	static int32 GetMillisecond( FDateTime A );

	/* Returns the minute component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetMinute"), Category="Math|DateTime")
	static int32 GetMinute( FDateTime A );

	/* Returns the month component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetMonth"), Category="Math|DateTime")
	static int32 GetMonth( FDateTime A );

	/* Returns the second component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetSecond"), Category="Math|DateTime")
	static int32 GetSecond( FDateTime A );

	/* Returns the time elapsed since midnight of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetTimeOfDay"), Category="Math|DateTime")
	static FTimespan GetTimeOfDay( FDateTime A );

	/* Returns the year component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetYear"), Category="Math|DateTime")
	static int32 GetYear( FDateTime A );

	/* Returns whether A's time is in the afternoon */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="IsAfternoon"), Category="Math|DateTime")
	static bool IsAfternoon( FDateTime A );

	/* Returns whether A's time is in the morning */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="IsMorning"), Category="Math|DateTime")
	static bool IsMorning( FDateTime A );

	/* Returns the number of days in the given year and month */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DaysInMonth"), Category="Math|DateTime")
	static int32 DaysInMonth( int32 Year, int32 Month );

	/* Returns the number of days in the given year */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="DaysInYear"), Category="Math|DateTime")
	static int32 DaysInYear( int32 Year );

	/* Returns whether given year is a leap year */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="IsLeapYear"), Category="Math|DateTime")
	static bool IsLeapYear( int32 Year );

	/* Returns the maximum date and time value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="MaxValue"), Category="Math|DateTime")
	static FDateTime DateTimeMaxValue( );

	/* Returns the minimum date and time value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="MinValue"), Category="Math|DateTime")
	static FDateTime DateTimeMinValue( );

	/* Returns the local date and time on this computer */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Now"), Category="Math|DateTime")
	static FDateTime Now( );

	/* Returns the local date on this computer */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Today"), Category="Math|DateTime")
	static FDateTime Today( );

	/* Returns the UTC date and time on this computer */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="UtcNow"), Category="Math|DateTime")
	static FDateTime UtcNow( );

	//
	// Timespan functions.
	//

	/* Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Timespan + Timespan", CompactNodeTitle="+", Keywords="+ add plus"), Category="Math|Timespan")
	static FTimespan Add_TimespanTimespan( FTimespan A, FTimespan B );

	/* Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Timespan - Timespan", CompactNodeTitle="-", Keywords="- subtract minus"), Category="Math|Timespan")
	static FTimespan Subtract_TimespanTimespan( FTimespan A, FTimespan B );

	/* Scalar multiplication (A * s) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Timespan * float", CompactNodeTitle="*", Keywords="* multiply"), Category="Math|Timespan")
	static FTimespan Multiply_TimespanFloat( FTimespan A, float Scalar );

	/* Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Equal (Timespan)", CompactNodeTitle="==", Keywords="== equal"), Category="Math|Timespan")
	static bool EqualEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/* Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="NotEqual (Timespan)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="Math|Timespan")
	static bool NotEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/* Returns true if A is Greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Timespan > Timespan", CompactNodeTitle=">", Keywords="> greater"), Category="Math|Timespan")
	static bool Greater_TimespanTimespan( FTimespan A, FTimespan B );

	/* Returns true if A is Greater than B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Timespan >= Timespan", CompactNodeTitle=">=", Keywords=">= greater"), Category="Math|Timespan")
	static bool GreaterEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/* Returns true if A is Greater than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Timespan < Timespan", CompactNodeTitle="<", Keywords="< less"), Category="Math|Timespan")
	static bool Less_TimespanTimespan( FTimespan A, FTimespan B );

	/* Returns true if A is Greater than B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="Timespan <= Timespan", CompactNodeTitle="<=", Keywords="<= less"), Category="Math|Timespan")
	static bool LessEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/* Returns the days component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetDays"), Category="Math|Timespan")
	static int32 GetDays( FTimespan A );

	/* Returns the absolute value of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetDuration"), Category="Math|Timespan")
	static FTimespan GetDuration( FTimespan A );

	/* Returns the hours component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetHours"), Category="Math|Timespan")
	static int32 GetHours( FTimespan A );

	/* Returns the milliseconds component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetMilliseconds"), Category="Math|Timespan")
	static int32 GetMilliseconds( FTimespan A );

	/* Returns the minutes component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetMinutes"), Category="Math|Timespan")
	static int32 GetMinutes( FTimespan A );

	/* Returns the seconds component of A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetSeconds"), Category="Math|Timespan")
	static int32 GetSeconds( FTimespan A );

	/* Returns the total number of days in A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetTotalDays"), Category="Math|Timespan")
	static float GetTotalDays( FTimespan A );

	/* Returns the total number of hours in A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetTotalHours"), Category="Math|Timespan")
	static float GetTotalHours( FTimespan A );

	/* Returns the total number of milliseconds in A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetTotalMilliseconds"), Category="Math|Timespan")
	static float GetTotalMilliseconds( FTimespan A );

	/* Returns the total number of minutes in A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetTotalMinutes"), Category="Math|Timespan")
	static float GetTotalMinutes( FTimespan A );

	/* Returns the total number of seconds in A */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="GetTotalSeconds"), Category="Math|Timespan")
	static float GetTotalSeconds( FTimespan A );

	/* Returns a time span that represents the specified number of days */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="FromDays"), Category="Math|Timespan")
	static FTimespan FromDays( float Days );

	/* Returns a time span that represents the specified number of hours */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="FromHours"), Category="Math|Timespan")
	static FTimespan FromHours( float Hours );

	/* Returns a time span that represents the specified number of milliseconds */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="FromMilliseconds"), Category="Math|Timespan")
	static FTimespan FromMilliseconds( float Milliseconds );

	/* Returns a time span that represents the specified number of minutes */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="FromMinutes"), Category="Math|Timespan")
	static FTimespan FromMinutes( float Minutes );

	/* Returns a time span that represents the specified number of seconds */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="FromSeconds"), Category="Math|Timespan")
	static FTimespan FromSeconds( float Seconds );

	/* Returns the maximum time span value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="MaxValue"), Category="Math|Timespan")
	static FTimespan TimespanMaxValue( );

	/* Returns the minimum time span value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="MinValue"), Category="Math|Timespan")
	static FTimespan TimespanMinValue( );

	/* Returns the ratio between two time spans (A / B), handles zero values */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="TimespanRatio"), Category="Math|Timespan")
	static float TimespanRatio( FTimespan A, FTimespan B );

	/* Returns a zero time span value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName="ZeroValue"), Category="Math|Timespan")
	static FTimespan TimespanZeroValue( );

	// -- Begin K2 utilities

	/** Converts a byte to a float */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToFloat (byte)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static float Conv_ByteToFloat(uint8 InByte);

	/** Converts an integer to a float */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToFloat (int)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static float Conv_IntToFloat(int32 InInt);

	/** Converts an integer to a byte (if the integer is too large, returns the low 8 bits) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToByte (int)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static uint8 Conv_IntToByte(int32 InInt);

	/** Converts a int to a bool*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToBool (int)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static bool Conv_IntToBool(int32 InInt);

	/** Converts a bool to an int */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToInt (bool)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static int32 Conv_BoolToInt(bool InBool);

	/** Converts a bool to a float (0.0f or 1.0f) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToFloat (bool)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static float Conv_BoolToFloat(bool InBool);

	/** Converts a bool to a byte */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToByte (bool)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static uint8 Conv_BoolToByte(bool InBool);
	
	/** Converts a byte to an integer */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToInt (byte)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static int32 Conv_ByteToInt(uint8 InByte);

	/** Converts a vector to LinearColor */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToLinearColor (vector)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FLinearColor Conv_VectorToLinearColor(FVector InVec);

	/** Converts a LinearColor to a vector */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToVector (linear color)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FVector Conv_LinearColorToVector(FLinearColor InLinearColor);

	/** Converts a color to LinearColor */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToLinearColor (color)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FLinearColor Conv_ColorToLinearColor(FColor InColor);

	/** Converts a LinearColor to a color*/
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToColor (linear color)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FColor Conv_LinearColorToColor(FLinearColor InLinearColor);

	/** Convert a vector to a transform. Uses vector as location */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToTransform (vector)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FTransform Conv_VectorToTransform(FVector InLocation);
	
	/** Convert a Vector to a Vector2D */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToVector2D (Vector)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FVector2D Conv_VectorToVector2D(FVector InVector);

	/** Convert a Vector2D to a Vector */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToVector (Vector2D)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FVector Conv_Vector2DToVector(FVector2D InVector2D, float Z = 0);

	/** Convert a float into a vector, where each element is that float */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToVector (float)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FVector Conv_FloatToVector(float InFloat);

	/** Convert a float into a LinearColor, where each element is that float */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToLinearColor (float)", CompactNodeTitle = "->", Keywords="cast convert"), Category="Math|Conversions")
	static FLinearColor Conv_FloatToLinearColor(float InFloat);

	/** Makes a vector {X, Y, Z} */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="construct build", NativeMakeFunc))
	static FVector MakeVector(float X, float Y, float Z);

	/** Breaks a vector apart into X, Y, Z */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(NativeBreakFunc))
	static void BreakVector(FVector InVec, float& X, float& Y, float& Z);

	/** Makes a 2d vector {X, Y} */
	UFUNCTION(BlueprintPure, Category="Math|Vector2D", meta=(Keywords="construct build", NativeMakeFunc))
	static FVector2D MakeVector2D(float X, float Y);

	/** Breaks a 2D vector apart into X, Y. */
	UFUNCTION(BlueprintPure, Category="Math|Vector2D", meta=(NativeBreakFunc))
	static void BreakVector2D(FVector2D InVec, float& X, float& Y);

	/** Rotate the world forward vector by the given rotation */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="rotation rotate"))
	static FVector GetForwardVector(FRotator InRot);

	/** Rotate the world right vector by the given rotation */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="rotation rotate"))
	static FVector GetRightVector(FRotator InRot);

	/** Rotate the world up vector by the given rotation */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="rotation rotate"))
	static FVector GetUpVector(FRotator InRot);

	/** Makes a rotator {Pitch, Yaw, Roll} */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator", NativeMakeFunc))
	static FRotator MakeRot(float Pitch, float Yaw, float Roll);
	
	/** Find a rotation for an object at Start location to point at Target location. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate"))
	static FRotator FindLookAtRotation(const FVector& Start, const FVector& Target);

	/** Builds a rotator given only a XAxis. Y and Z are unspecified but will be orthonormal. XAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromX(const FVector& X);

	/** Builds a rotation matrix given only a YAxis. X and Z are unspecified but will be orthonormal. YAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromY(const FVector& Y);

	/** Builds a rotation matrix given only a ZAxis. X and Y are unspecified but will be orthonormal. ZAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZ(const FVector& Z);

	/** Builds a matrix with given X and Y axes. X will remain fixed, Y may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromXY(const FVector& X, const FVector& Y);

	/** Builds a matrix with given X and Z axes. X will remain fixed, Z may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromXZ(const FVector& X, const FVector& Z);

	/** Builds a matrix with given Y and X axes. Y will remain fixed, X may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromYX(const FVector& Y, const FVector& X);

	/** Builds a matrix with given Y and Z axes. Y will remain fixed, Z may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromYZ(const FVector& Y, const FVector& Z);

	/** Builds a matrix with given Z and X axes. Z will remain fixed, X may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZX(const FVector& Z, const FVector& X);

	/** Builds a matrix with given Z and Y axes. Z will remain fixed, Y may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZY(const FVector& Z, const FVector& Y);

	/** Breaks apart a rotator into Pitch, Yaw, Roll */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate rotator breakrotator", NativeBreakFunc))
	static void BreakRot(FRotator InRot, float& Pitch, float& Yaw, float& Roll);

	/** Breaks apart a rotator into its component axes */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate rotator breakrotator"))
	static void BreakRotIntoAxes(const FRotator& InRot, FVector& X, FVector& Y, FVector& Z);

	/** Make a transform from location, rotation and scale */
	UFUNCTION(BlueprintPure, meta=(Scale = "1,1,1", Keywords="construct build", NativeMakeFunc), Category="Math|Transform")
	static FTransform MakeTransform(FVector Location, FRotator Rotation, FVector Scale);

	/** Breaks apart a transform into location, rotation and scale */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(NativeBreakFunc))
	static void BreakTransform(const FTransform& InTransform, FVector& Location, FRotator& Rotation, FVector& Scale);

	/** Makes a SRand-based random number generator */
	UFUNCTION(BlueprintPure, meta = (Keywords = "construct build", NativeMakeFunc), Category = "Math|Random")
		static FRandomStream MakeRandomStream(int32 InitialSeed);

	/** Breaks apart a random number generator */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (NativeBreakFunc))
		static void BreakRandomStream(const FRandomStream& InRandomStream, int32& InitialSeed);

	/** Make a color from individual color components (RGB space) */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(Keywords="construct build"))
	static FLinearColor MakeColor(float R, float G, float B, float A = 1.0f);

	/** Breaks apart a color into individual RGB components (as well as alpha) */
	UFUNCTION(BlueprintPure, Category="Math|Color")
	static void BreakColor(const FLinearColor InColor, float& R, float& G, float& B, float& A);

	/** Make a color from individual color components (HSV space) */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(FriendlyName = "HSV to RGB"))
	static FLinearColor HSVToRGB(float H, float S, float V, float A = 1.0f);

	/** Breaks apart a color into individual HSV components (as well as alpha) */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(FriendlyName = "RGB to HSV"))
	static void RGBToHSV(const FLinearColor InColor, float& H, float& S, float& V, float& A);

	// Converts a HSV linear color (where H is in R, S is in G, and V is in B) to RGB
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(FriendlyName = "HSV to RGB (vector)", Keywords="cast convert"))
	static void HSVToRGB_Vector(const FLinearColor HSV, FLinearColor& RGB);

	// Converts a RGB linear color to HSV (where H is in R, S is in G, and V is in B)
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(FriendlyName = "RGB to HSV (vector)", Keywords="cast convert"))
	static void RGBToHSV_Vector(const FLinearColor RGB, FLinearColor& HSV);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Utilities|String")
	static FString SelectString(const FString& A, const FString& B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static int32 SelectInt(int32 A, int32 B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float SelectFloat(float A, float B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector SelectVector(FVector A, FVector B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate"))
	static FRotator SelectRotator(FRotator A, FRotator B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Color")
	static FLinearColor SelectColor(FLinearColor A, FLinearColor B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FTransform SelectTransform(const FTransform& A, const FTransform& B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Utilities")
	static UObject* SelectObject(UObject* A, UObject* B, bool bSelectA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static UClass* SelectClass(UClass* A, UClass* B, bool bSelectA);

	// Build a reference frame from three axes
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate"))
	static FRotator MakeRotationFromAxes(FVector Forward, FVector Right, FVector Up);

	/** Create a rotator which orients X along the supplied direction vector */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "RotationFromXVector", Keywords="rotation rotate cast convert"), Category="Math|Rotator")
	static FRotator Conv_VectorToRotator(FVector InVec);

	/** Get the X direction vector after this rotation */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "GetRotationXVector", Keywords="rotation rotate cast convert"), Category="Math|Rotator")
	static FVector Conv_RotatorToVector(FRotator InRot);


	//
	// Object operators and functions.
	//
	
	/* Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (Object)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities")
	static bool EqualEqual_ObjectObject(class UObject* A, class UObject* B);

	/* Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual (Object)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities")
	static bool NotEqual_ObjectObject(class UObject* A, class UObject* B);

	//
	// Class operators and functions.
	//

	/* Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (Class)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities")
	static bool EqualEqual_ClassClass(class UClass* A, class UClass* B);

	/* Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual (Class)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities")
	static bool NotEqual_ClassClass(class UClass* A, class UClass* B);

	/**
	 * Determine if a class is a child of another class.
	 *
	 * @return	true if TestClass == ParentClass, or if TestClass is a child of ParentClass; false otherwise, or if either
	 *			the value for either parameter is 'None'.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities")
	static bool ClassIsChildOf(TSubclassOf<class UObject> TestClass, TSubclassOf<class UObject> ParentClass);

	//
	// Name operators.
	//
	
	/* Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (Name)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities|Name")
	static bool EqualEqual_NameName(FName A, FName B);

	/* Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual (Name)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities|Name")
	static bool NotEqual_NameName(FName A, FName B);

	//
	// Transform functions
	//
	
	/** 
	 *	Transform a position by the supplied transform.
	 *	For example, if T was an object's transform, would transform a position from local space to world space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(Keywords="location"))
	static FVector TransformLocation(const FTransform& T, FVector Location);

	/** 
	 *	Transform a direction vector by the supplied transform - will not change its length. 
	 *	For example, if T was an object's transform, would transform a direction from local space to world space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FVector TransformDirection(const FTransform& T, FVector Direction);

	/** 
	 *	Transform a position by the inverse of the supplied transform.
	 *	For example, if T was an object's transform, would transform a position from world space to local space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(Keywords="location"))
	static FVector InverseTransformLocation(const FTransform& T, FVector Location);

	/** 
	 *	Transform a direction vector by the inverse of the supplied transform - will not change its length.
	 *	For example, if T was an object's transform, would transform a direction from world space to local space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FVector InverseTransformDirection(const FTransform& T, FVector Direction);

	/**
	 * Compose two transforms in order: A * B.
	 *
	 * Order matters when composing transforms:
	 * A * B will yield a transform that logically first applies A then B to any subsequent transformation.
	 *
	 * Example: LocalToWorld = ComposeTransforms(DeltaRotation, LocalToWorld) will change rotation in local space by DeltaRotation.
	 * Example: LocalToWorld = ComposeTransforms(LocalToWorld, DeltaRotation) will change rotation in world space by DeltaRotation.
	 *
	 * @return New transform: A * B
	 */
	UFUNCTION(BlueprintPure, meta=(Keywords="multiply *"), Category="Math|Transform")
	static FTransform ComposeTransforms(const FTransform& A, const FTransform& B);

	/** 
	 *  Convert a world-transform from world-space into local-space.
	 *  @param		WorldTransform	The transform you wish to convert
	 *  @param		LocalTransform	The transform the conversion is relative to
	 *  @return		A new relative transform
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(Keywords="cast convert"))
	static FTransform ConvertTransformToRelative(const FTransform& WorldTransform, const FTransform& LocalTransform);

	/* Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1). */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Lerp (Transform)"), Category="Math|Transform")
	static FTransform TLerp(const FTransform& A, const FTransform& B, float Alpha);

	/** Ease between A and B using a specified easing function. */
	UFUNCTION(BlueprintPure, meta = (FriendlyName = "Ease (Transform)", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static FTransform TEase(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Tries to reach a target transform. */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static FTransform TInterpTo(const FTransform& Current, const FTransform& Target, float DeltaTime, float InterpSpeed);

	//
	// Vector2D functions
	//

	/** Returns addition of Vector A and Vector B (A + B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector2d + vector2d", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector2D")
	static FVector2D Add_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns subtraction of Vector B from Vector A (A - B) */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector2d - vector2d", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector2D")
	static FVector2D Subtract_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A scaled by B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector2d * float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Vector2D")
	static FVector2D Multiply_Vector2DFloat(FVector2D A, float B);

	/** Returns Vector A divided by B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector2d / float", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Vector2D")
	static FVector2D Divide_Vector2DFloat(FVector2D A, float B);

	/** Returns Vector A added by B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector2d + float", CompactNodeTitle = "+", Keywords = "+ add plus"), Category="Math|Vector2D")
	static FVector2D Add_Vector2DFloat(FVector2D A, float B);

	/** Returns Vector A subtracted by B */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "vector2d - float", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector2D")
	static FVector2D Subtract_Vector2DFloat(FVector2D A, float B);

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static float FInterpTo_Constant(float Current, float Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="position"))
	static FVector VInterpTo(FVector Current, FVector Target, float DeltaTime, float InterpSpeed);


	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Interpolation", meta = (Keywords = "position"))
	static FVector VInterpTo_Constant(FVector Current, FVector Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="position"))
	static FVector2D Vector2DInterpTo(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed);
	
	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="position"))
	static FVector2D Vector2DInterpTo_Constant(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed);
	
	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual rotation
	 * @param		Target			Target rotation
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="rotation rotate"))
	static FRotator RInterpTo(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual rotation
	 * @param		Target			Target rotation
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="rotation rotate"))
	static FRotator RInterpTo_Constant(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed);

	/**
	 * Interpolates towards a varying target color smoothly.
	 *
	 * @param		Current			Current Color
	 * @param		Target			Target Color
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated Color
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Interpolation", meta = (Keywords = "color"))
	static FLinearColor CInterpTo(FLinearColor Current, FLinearColor Target, float DeltaTime, float InterpSpeed);

	//
	// Random stream functions
	//

	/* Returns a uniformly distributed random number between 0 and Max - 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomIntegerFromStream(int32 Max, const FRandomStream& Stream);

	/** Return a random integer between Min and Max (>= Min and <= Max) */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomIntegerInRangeFromStream(int32 Min, int32 Max, const FRandomStream& Stream);

	/* Returns a random bool */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static bool RandomBoolFromStream(const FRandomStream& Stream);

	/** Returns a random float between 0 and 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloatFromStream(const FRandomStream& Stream);

	/** Generate a random number between Min and Max */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloatInRangeFromStream(float Min, float Max, const FRandomStream& Stream);

	/* Returns a random vector with length of 1.0 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static FVector RandomUnitVectorFromStream(const FRandomStream& Stream);

	/** Create a random rotation */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static FRotator RandomRotatorFromStream(bool bRoll, const FRandomStream& Stream);

	/** Reset a random stream */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void ResetRandomStream(const FRandomStream& Stream);

	/** Create a new random seed for a random stream */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void SeedRandomStream(UPARAM(ref) FRandomStream& Stream);

	/** Set the seed of a random stream to a specific number */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void SetRandomStreamSeed(UPARAM(ref) FRandomStream& Stream, int32 NewSeed);

	//
	// Geometry
	//

	/**  
	 * Finds the minimum area rectangle that encloses all of the points in InVerts
	 * Uses algorithm found in http://www.geometrictools.com/Documentation/MinimumAreaRectangle.pdf
	 *	
	 * @param		InVerts	- Points to enclose in the rectangle
	 * @outparam	OutRectCenter - Center of the enclosing rectangle
	 * @outparam	OutRectSideA - Vector oriented and sized to represent one edge of the enclosing rectangle, orthogonal to OutRectSideB
	 * @outparam	OutRectSideB - Vector oriented and sized to represent one edge of the enclosing rectangle, orthogonal to OutRectSideA
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Math|Geometry", meta=(WorldContext="WorldContextObject"))
	static void MinimumAreaRectangle(UObject* WorldContextObject, const TArray<FVector>& InVerts, const FVector& SampleSurfaceNormal, FVector& OutRectCenter, FRotator& OutRectRotation, float& OutSideLengthX, float& OutSideLengthY, bool bDebugDraw = false);

	/**
	 * Determines whether a given set of points are coplanar, with a tolerance. Any three points or less are always coplanar.
	 *
	 * @param Points - The set of points to determine coplanarity for.
	 * @param Tolerance - Larger numbers means more variance is allowed.
	 *
	 * @return Whether the points are relatively coplanar, based on the tolerance
	 */
	UFUNCTION(BlueprintCallable, Category = "Math|Geometry")
	static bool PointsAreCoplanar(const TArray<FVector>& Points, float Tolerance = 0.1f);

	//
	// Intersection
	//

	/**
	 * Computes the intersection point between a line and a plane.
	 * @param		T - The t of the intersection between the line and the plane
	 * @param		Intersection - The point of intersection between the line and the plane
	 * @return		True if the intersection test was successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Math|Intersection")
	static bool LinePlaneIntersection(const FVector& LineStart, const FVector& LineEnd, const FPlane& APlane, float& T, FVector& Intersection);

	/**
	 * Computes the intersection point between a line and a plane.
	 * @param		T - The t of the intersection between the line and the plane
	 * @param		Intersection - The point of intersection between the line and the plane
	 * @return		True if the intersection test was successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Math|Intersection", meta = (FriendlyName = "Line Plane Intersection (Origin & Normal)"))
	static bool LinePlaneIntersection_OriginNormal(const FVector& LineStart, const FVector& LineEnd, FVector PlaneOrigin, FVector PlaneNormal, float& T, FVector& Intersection);
};
