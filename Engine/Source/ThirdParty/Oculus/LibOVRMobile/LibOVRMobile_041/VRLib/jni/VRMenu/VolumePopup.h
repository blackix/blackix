/************************************************************************************

Filename    :   VolumePopup.h
Content     :   Popup dialog for when user changes sound volume.
Created     :   September 18, 2014
Authors     :   Jim Dos�

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VolumePopup_h )
#define OVR_VolumePopup_h

#include "VRMenu.h"

namespace OVR {

//==============================================================
// OvrVolumePopup
class OvrVolumePopup : public VRMenu
{
public:
	static const int		NumVolumeTics;

	static	VRMenuId_t		ID_BACKGROUND;
	static	VRMenuId_t		ID_VOLUME_ICON;
	static	VRMenuId_t		ID_VOLUME_TEXT;
	static	VRMenuId_t		ID_VOLUME_TICKS;

	static const char *		MENU_NAME;

	static const double		VolumeMenuFadeDelay;

    OvrVolumePopup();
    virtual ~OvrVolumePopup();

    // only one of these every needs to be created
    static  OvrVolumePopup * Create( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font );

    void					ShowVolume( App * app, const int current, const int max );

private:
	Vector3f				VolumeTextOffset;
	double					VolumeFadeTime;

private:
    // overloads
    virtual void    Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
                                    BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId );
	virtual bool	OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType );

	void			CreateSubMenus( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font );
};

} // namespace OVR

#endif // OVR_VolumePopup_h
