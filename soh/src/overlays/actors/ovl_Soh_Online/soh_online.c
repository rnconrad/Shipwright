#include "soh_online.h"

#define FLAGS (ACTOR_FLAG_4 | ACTOR_FLAG_5)

void Soh_Online_Init(Actor* thisx, GlobalContext* globalCtx);
void Soh_Online_Destroy(Actor* thisx, GlobalContext* globalCtx);
void Soh_Online_Update(Actor* thisx, GlobalContext* globalCtx);
void Soh_Online_Draw(Actor* thisx, GlobalContext* globalCtx);

const ActorInit Soh_Online_InitVars = {
    ACTOR_SOH_ONLINE,
    ACTORCAT_MISC,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(Soh_Online),
    (ActorFunc)Soh_Online_Init,
    (ActorFunc)Soh_Online_Destroy,
    (ActorFunc)Soh_Online_Update,
    (ActorFunc)Soh_Online_Draw,
};

void Soh_Online_Init(Actor* thisx, GlobalContext* globalCtx) {

}

void Soh_Online_Destroy(Actor* thisx, GlobalContext* globalCtx) {

}

void Soh_Online_Update(Actor* thisx, GlobalContext* globalCtx) {

}

void Soh_Online_Draw(Actor* thisx, GlobalContext* globalCtx) {
    GfxPrint printer;
    Gfx* gfx;

    OPEN_DISPS(globalCtx->state.gfxCtx, __FILE__, __LINE__);

    gfx = POLY_OPA_DISP + 1;
    gSPDisplayList(OVERLAY_DISP++, gfx);

    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);

    GfxPrint_SetColor(&printer, 255, 0, 255, 255);
    GfxPrint_SetPos(&printer, 10, 10);
    GfxPrint_Printf(&printer, "Hello from MyActor_Draw");

    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);

    gSPEndDisplayList(gfx++);
    gSPBranchList(POLY_OPA_DISP, gfx);
    POLY_OPA_DISP = gfx;

    CLOSE_DISPS(globalCtx->state.gfxCtx, __FILE__, __LINE__);
}