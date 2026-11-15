// GeoBR GDPS hooks into GD's account screens. The server redirect and the
// separate save live in the companion mod geobr.gdps; here only the register
// screen is pre-filled and the GDPS login is linked to the GeoBR profile.
#include "GdpsRouting.hpp"
#include "GdpsService.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCTextInputNode.hpp>
#include <Geode/modify/AccountLoginLayer.hpp>
#include <Geode/modify/AccountRegisterLayer.hpp>
#include <Geode/modify/GJAccountManager.hpp>
#ifdef GEODE_IS_WINDOWS
#include <Geode/modify/AccountHelpLayer.hpp>
#endif

using namespace geode::prelude;
using namespace geobr;

// GeoBR accounts need no e-mail: GD's own form still asks for one, so it is
// filled with a reserved address the server ignores.
class $modify(GeoBRGdpsRegister, AccountRegisterLayer) {
    bool init() {
        if (!AccountRegisterLayer::init()) return false;
        if (!gdps::enabled()) return true;
        constexpr char const* placeholder = "sem-email@geobr.invalid";
        if (m_emailField) m_emailField->setString(placeholder);
        if (m_verifyEmailField) m_verifyEmailField->setString(placeholder);
        auto note = CCLabelBMFont::create("Conta GeoBR GDPS: o e-mail nao e necessario", "chatFont.fnt");
        note->setScale(.55f);
        note->setColor({255, 226, 120});
        note->setID("geobr-gdps-register-note");
        auto win = CCDirector::sharedDirector()->getWinSize();
        note->setPosition({win.width / 2.f, 12.f});
        m_mainLayer->addChild(note, 50);
        return true;
    }
};

// After logging in, the GDPS account and this device's GeoBR profile are linked.
class $modify(GeoBRGdpsLogin, AccountLoginLayer) {
    void loginAccountFinished(int accountID, int userID) {
        AccountLoginLayer::loginAccountFinished(accountID, userID);
        if (gdps::enabled())
            Loader::get()->queueInMainThread([] { gdps::linkProfileAfterLogin(); gdps::refreshMe(); });
    }
};

// Logging out of a GDPS account goes back to the device's guest profile.
#ifdef GEODE_IS_WINDOWS
// On Windows GJAccountManager::unlinkFromAccount is inlined; GD logs out from
// AccountHelpLayer's confirmation alert, so the account is checked around it.
class $modify(GeoBRGdpsAccountHelp, AccountHelpLayer) {
    void FLAlert_Clicked(FLAlertLayer* layer, bool btn2) {
        bool wasGdps = gdps::enabled() && GJAccountManager::get()->m_accountID > 0;
        AccountHelpLayer::FLAlert_Clicked(layer, btn2);
        if (wasGdps && GJAccountManager::get()->m_accountID <= 0) gdps::restoreGuestProfile();
    }
};
#else
class $modify(GeoBRGdpsAccount, GJAccountManager) {
    void unlinkFromAccount() {
        bool wasGdps = gdps::enabled() && m_accountID > 0;
        GJAccountManager::unlinkFromAccount();
        if (wasGdps) gdps::restoreGuestProfile();
    }
};
#endif
