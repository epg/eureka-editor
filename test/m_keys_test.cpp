//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2021 Ioan Chera
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "m_keys.h"
#include "testUtils/TempDirContext.hpp"
#include "Instance.h"

void DLG_Notify(EUR_FORMAT_STRING(const char *msg), ...)
{
}

void updateMenuBindings()
{
}

void Instance::Status_Set(EUR_FORMAT_STRING(const char *fmt), ...) const
{
}

//========================================================================

//
// Fixture
//
class MKeys : public TempDirContext
{
protected:
    void SetUp() override
    {
        TempDirContext::SetUp();
        static bool loaded;
        if(!loaded)
        {
            static editor_command_t commands[] =
            {
                { "BR_ClearSearch", "Browser", nullptr },
                { "BR_Scroll", "Browser", nullptr },
                { "3D_NAV_Left", NULL, nullptr },
                { "3D_NAV_Right", NULL, nullptr },
                { "LIN_SelectPath", NULL, nullptr },
                { "GivenFile",  "File", nullptr },
                { "Insert",    "Edit", nullptr },
                { "Delete",    "Edit", nullptr },
                { "Mirror",    "General", nullptr },
            };
            M_RegisterCommandList(commands);
            loaded = true;
        }
        global::home_dir = mTempDir;
        global::install_dir = getChildPath("install");
        ASSERT_TRUE(FileMakeDir(global::install_dir));
        mDeleteList.push(global::install_dir);

        writeBindingsFile();
        M_LoadBindings();

    }

    void TearDown() override
    {
        global::config_file.clear();
        global::install_dir.clear();
        global::home_dir.clear();
        TempDirContext::TearDown();
    }
private:
    void writeBindingsFile();
};

//
// Write the bindings file
//
void MKeys::writeBindingsFile()
{
    FILE *f = fopen((global::install_dir + "/bindings.cfg").c_str(), "wt");
    ASSERT_NE(f, nullptr);
    mDeleteList.push(global::install_dir + "/bindings.cfg");

    fprintf(f, "browser    CMD-k    BR_ClearSearch\n");
    fprintf(f, "browser    PGUP    BR_Scroll    -3\n");
    fprintf(f, "browser    PGDN    BR_Scroll    +3\n");
    fprintf(f, "render    ALT-LEFT    3D_NAV_Left    384\n");
    fprintf(f, "render    ALT-RIGHT    3D_NAV_Right    384\n");
    fprintf(f, "line    E    LIN_SelectPath    /sametex\n");
    fprintf(f, "general    META-n    GivenFile    next\n");
    fprintf(f, "general    CMD-SPACE    Insert    /nofill\n");

    int n = fclose(f);
    ASSERT_EQ(n, 0);

    f = fopen((global::home_dir + "/bindings.cfg").c_str(), "wt");
    ASSERT_NE(f, nullptr);
    mDeleteList.push(global::home_dir + "/bindings.cfg");

    fprintf(f, "general SHIFT-DEL    Delete    /keep\n");
    fprintf(f, "general    SHIFT-BS    Delete    /keep\n");
    fprintf(f, "general    CMD-k    Mirror    horiz\n");

    n = fclose(f);
    ASSERT_EQ(n, 0);
}

TEST_F(MKeys, MKeyToString)
{
    ASSERT_EQ(M_KeyToString(EMOD_COMMAND | 'a'), "CMD-a");
    ASSERT_EQ(M_KeyToString(EMOD_SHIFT | 'a'), "A");
    ASSERT_EQ(M_KeyToString(EMOD_SHIFT | FL_Page_Up), "SHIFT-PGUP");
    ASSERT_EQ(M_KeyToString(EMOD_META | FL_Page_Down), "META-PGDN");
}

TEST_F(MKeys, MIsKeyBound)
{
    ASSERT_TRUE(M_IsKeyBound(FL_Page_Up, KCTX_Browser));
    ASSERT_TRUE(M_IsKeyBound(EMOD_COMMAND | 'k', KCTX_Browser));
    ASSERT_TRUE(M_IsKeyBound(EMOD_COMMAND | 'k', KCTX_General));
    ASSERT_TRUE(M_IsKeyBound(EMOD_SHIFT | 'e', KCTX_Line));
    ASSERT_FALSE(M_IsKeyBound(EMOD_COMMAND | 'k', KCTX_Render));
}

TEST_F(MKeys, MRemoveBinding)
{
    ASSERT_TRUE(M_IsKeyBound(EMOD_SHIFT | FL_BackSpace, KCTX_General));
    M_RemoveBinding(EMOD_SHIFT | FL_BackSpace, KCTX_Vertex);
    ASSERT_TRUE(M_IsKeyBound(EMOD_SHIFT | FL_BackSpace, KCTX_General));
    M_RemoveBinding(EMOD_SHIFT | FL_BackSpace, KCTX_General);
    ASSERT_FALSE(M_IsKeyBound(EMOD_SHIFT | FL_BackSpace, KCTX_General));
}
