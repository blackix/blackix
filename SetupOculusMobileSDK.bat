@ECHO   Installing...
@rmdir /s /q Engine\Source\ThirdParty\Oculus\LibOVRMobile >nul 2>&1
@xcopy /s /q /y Engine\Source\ThirdParty\Oculus\OculusMobile\SDK_1_0_0\Libs\*.jar Engine\Build\Android\Java\libs

@if %errorlevel% neq 0 goto Fail

@ECHO Completed!
@goto End

:Fail
@ECHO Error, can't copy files?

:End