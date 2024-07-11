// Dear ImGui: standalone example application for DirectX 9

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h>
#include <fstream>
#include <json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

using json = nlohmann::json;

// Data
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static bool                     g_DeviceLost = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

struct Response {
    std::string reply = "";
    std::string content = "";
    int health = 0;
};

enum Relationship {
    POSITIVE,
    NEUTERAL,
    NEGATIVE
};

struct Message {
    std::string content = "";
    int timeToRespond = 20;
    Response responce[2];
    Relationship relationship = NEUTERAL;
};

struct Character {
    std::string name = "";
    std::vector<Message> messages;
    std::vector<std::string> dummyMessages = {};
};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int DoesCharacterExist(std::vector<Character> characters, std::string name);
void Save(std::vector<Character> characters);

// Main code
int main(int, char**)
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Message Editor", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    std::ifstream f("Content/Data/messages.json");
    //std::ifstream f("test.json");
    json data;
    if (f.good()) {
        data = json::parse(f);
    }


    //list of characters
    std::vector<Character> characters;
    std::vector<std::string> treeNames;
    const char* relationahipListItems[] = { "Positive", "Neutral", "Negative" };

    for (auto& messages : data["messages"]) {
        //get the next entry

        //should be fine to save it the first time and just check the ne=xt becuas erhtey should all be in order

        Message newMessage;

        if (messages["content"].is_null()) {
            newMessage.content = "";
        }
        else {
            newMessage.content = messages["content"];
        }

        if (messages["timeToRespond"].is_null()) {
            newMessage.timeToRespond = 20;
        }
        else {
            newMessage.timeToRespond = messages["timeToRespond"];
        }

        if (messages["relationship"].is_null()) {
            newMessage.relationship = NEUTERAL;
        }
        else {
            newMessage.relationship = messages["relationship"];
        }

        int responseTrack = 0;
        for (auto& response : messages["responses"]) {
            if (response["reply"].is_null()) {
                newMessage.responce[responseTrack].reply = "";
            }
            else {
                newMessage.responce[responseTrack].reply = response["reply"];
            }

            if (response["content"].is_null()) {
                newMessage.responce[responseTrack].content = "";
            }
            else {
                newMessage.responce[responseTrack].content = response["content"];
            }

            if (response["health"].is_null()) {
                newMessage.responce[responseTrack].health = 0;
            }
            else {
                newMessage.responce[responseTrack].health = response["health"];
            }
            responseTrack++;
        }

        int charLoc = DoesCharacterExist(characters, messages["from"]);
        if (charLoc != -1) {
            characters.at(charLoc).messages.push_back(newMessage);
        }
        else {
            Character newCharacter;
            newCharacter.name = messages["from"];
            newCharacter.messages.push_back(newMessage);
            characters.push_back(newCharacter);
            treeNames.push_back(newCharacter.name);
        }

    }

    //std::cout << "Dummy Messages\n";

    for (auto& dMessages : data["dummyMessages"]) {
        
        int charLoc = DoesCharacterExist(characters, dMessages["from"]);
        
        if (charLoc == -1) {
            //no exists
            Character newCharacter;
            newCharacter.name = dMessages["from"];
            newCharacter.dummyMessages.push_back(dMessages["content"]);
            characters.push_back(newCharacter);
            treeNames.push_back(newCharacter.name);
        }
        else {
            //yes exists
            characters[charLoc].dummyMessages.push_back(dMessages["content"]);
        }
    }

    bool done = false;
    while (!done) {

        // Poll and handle messages (inputs, window resize, etc.)
       // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle lost D3D9 device
        if (g_DeviceLost)
        {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            g_DeviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        RECT rect;
        GetClientRect(hwnd, &rect);
        ImVec2 windowSize(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top));

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(windowSize);

        //// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        //if (show_demo_window)
        //    ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Message Editor", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);                          // Create a window called "Hello, world!" and append into it.
            
            float windowWidth = ImGui::GetWindowWidth();
            ImGui::PushItemWidth(windowWidth - 200);

            if (ImGui::Button("Save") || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)))
            {
                std::cout << "\033[32m" << "saved!\n";
                Save(characters);
            }
            ImGui::SameLine();
            if (ImGui::Button("New Character")) {
                Character newCharacter;
                newCharacter.name = "New character";
                characters.push_back(newCharacter);
                treeNames.push_back(newCharacter.name);
            }
            ImGui::SameLine();
            if (ImGui::Button("Update tree names"))
            {
                for (int i = 0; i < characters.size(); i++) {
                    treeNames[i] = characters[i].name;
                }
            }

            //ImGui::Text("This is some useful text."); 
            //ImGui::Text(data);

            for (int i = 0; i < characters.size(); i++) {

                if (ImGui::TreeNode(treeNames.at(i).c_str())) {
                    char characterName[300] = { 0 };
                    strncpy_s(characterName, characters[i].name.c_str(), sizeof(characterName));
                    ImGui::PushID(i);
                    if (ImGui::InputText("Name", characterName, IM_ARRAYSIZE(characterName))){
                        characters[i].name = characterName;
                        //treeNames[i] = characterName;
                    }
                    //ImGui::PopID();

                    //ImGui::Dummy(ImVec2(0.0f, 5.0f));

                    if (ImGui::TreeNode("Messages")) {

                        if (ImGui::Button("New Message")) {
                            Message newMessage;
                            characters[i].messages.push_back(newMessage);
                        }

                        ImGui::Dummy(ImVec2(0.0f, 5.0f));

                        for (int j = 0; j < characters[i].messages.size(); j++) {

                            characters[i].messages[j].content;

                            char messageContent[3000] = { 0 };
                            strncpy_s(messageContent, characters[i].messages[j].content.c_str(), sizeof(messageContent));

                            ImGui::PushID(j);
                            if (ImGui::InputText("Content", messageContent, IM_ARRAYSIZE(messageContent))) {
                                characters[i].messages[j].content = messageContent;
                            }
                            //ImGui::PopID();

                            int TimeToRespondContent = characters[i].messages[j].timeToRespond;
                            //ImGui::PushID(j);
                            if (ImGui::InputInt("Time to respond", &TimeToRespondContent)) {
                                characters[i].messages[j].timeToRespond = TimeToRespondContent;
                            }
                            //ImGui::PopID();

                            //ImGui::PushID(j);
                            int currentItem = characters[i].messages[j].relationship;
                            if (ImGui::Combo("Relationship", &currentItem, relationahipListItems, IM_ARRAYSIZE(relationahipListItems))) {
                                try {
                                    if (currentItem >= 0 && currentItem <= 3) {
                                        characters[i].messages[j].relationship = static_cast<Relationship>(currentItem);
                                        //std::cout << "thing callse ";
                                    }
                                    else {
                                        throw (currentItem);
                                    }
                                }
                                catch (int badNum) {
                                    std::cout << "\033[31m" << "Relationship Enum failed cast, num is " << badNum << ". Number shoud be 1 - 3, check it is in json" << "\033[0m" << "\n";
                                }
                            }
                            ImGui::PopID();

                            char responceReply1[3000] = { 0 };
                            strncpy_s(responceReply1, characters[i].messages[j].responce[0].reply.c_str(), sizeof(responceReply1));
                            char responceReply2[3000] = { 0 };
                            strncpy_s(responceReply2, characters[i].messages[j].responce[1].reply.c_str(), sizeof(responceReply2));

                            char responceContent1[3000] = { 0 };
                            strncpy_s(responceContent1, characters[i].messages[j].responce[0].content.c_str(), sizeof(responceContent1));
                            char responceContent2[3000] = { 0 };
                            strncpy_s(responceContent2, characters[i].messages[j].responce[1].content.c_str(), sizeof(responceContent2));

                            ImGui::PushID(j);
                            if (ImGui::TreeNode("Responses")) {
                                //ImGui::PopID();
                                if (ImGui::InputText("Content 1", responceContent1, IM_ARRAYSIZE(responceContent1))) {
                                    characters[i].messages[j].responce[0].content = responceContent1;
                                }

                                if (ImGui::InputText("reply 1", responceReply1, IM_ARRAYSIZE(responceReply1))) {
                                    characters[i].messages[j].responce[0].reply = responceReply1;
                                }

                                int health1 = characters[i].messages[j].responce[0].health;
                                if (ImGui::InputInt("Health 1", &health1)) {
                                    characters[i].messages[j].responce[0].health = health1;
                                }

                                if (ImGui::InputText("Content 2", responceContent2, IM_ARRAYSIZE(responceContent2))) {
                                    characters[i].messages[j].responce[1].content = responceContent2;
                                }

                                if (ImGui::InputText("reply 2", responceReply2, IM_ARRAYSIZE(responceReply2))) {
                                    characters[i].messages[j].responce[1].reply = responceReply2;
                                }

                                int health2 = characters[i].messages[j].responce[1].health;
                                if (ImGui::InputInt("Health 2", &health2)) {
                                    characters[i].messages[j].responce[1].health = health2;
                                }

                                ImGui::TreePop();
                            }

                            if (ImGui::Button("Delete Message")) {
                                characters[i].messages.erase(characters[i].messages.begin() + j);
                            }

                            ImGui::Dummy(ImVec2(0.0f, 5.0f));

                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNode("Dummy messages")) {

                        if (ImGui::Button("New Message")) {
                            std::string newDMessage;
                            characters[i].dummyMessages.push_back(newDMessage);
                        }

                        if (!characters[i].dummyMessages.empty()) {
                            for (int j = 0; j < characters[i].dummyMessages.size(); j++) {
                                char dummyContent[3000] = { 0 };
                                strncpy_s(dummyContent, characters[i].dummyMessages[j].c_str(), sizeof(dummyContent));

                                ImGui::PushID(j);
                                if (ImGui::InputText("Content", dummyContent, IM_ARRAYSIZE(dummyContent))) {
                                    characters[i].dummyMessages[j] = dummyContent;
                                }

                                if (ImGui::Button("Delete Message")) {
                                    characters[i].dummyMessages.erase(characters[i].dummyMessages.begin() + j);
                                }
                                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                                ImGui::PopID();
                            }
                        }

                        ImGui::TreePop();
                    }


                    if (ImGui::Button("Delete Character")) {
                        characters.erase(characters.begin() + i);
                        treeNames.erase(treeNames.begin() + i);
                    }

                    ImGui::PopID();

                    ImGui::Dummy(ImVec2(0.0f, 10.0f));

                    ImGui::TreePop();
                }
            }

            ImGui::PopItemWidth();
            ImGui::End();
        }

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            g_DeviceLost = true;

    }

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

int DoesCharacterExist(std::vector<Character> characters, std::string name) {

    for (int i = 0; i < characters.size(); i++) {
        if (characters[i].name == name) {
            return i;
        }
    }

    return -1;
}

void Save(std::vector<Character> characters) {
    json saveFile;

    for (auto& character : characters) {
        json jMessage;
        for (auto& charMessage : character.messages) {
            jMessage["from"] = character.name;
            jMessage["content"] = charMessage.content;
            jMessage["timeToRespond"] = charMessage.timeToRespond;
            jMessage["relationship"] = charMessage.relationship;

            json jResponse[2];
            for (int i = 0; i < 2; i++) {
                jResponse[i]["reply"] = charMessage.responce[i].reply;
                jResponse[i]["content"] = charMessage.responce[i].content;
                jResponse[i]["health"] = charMessage.responce[i].health;
            }
            jMessage["responses"].push_back(jResponse[0]);
            jMessage["responses"].push_back(jResponse[1]);
            jResponse[0].clear();
            jResponse[1].clear();
            saveFile["messages"].push_back(jMessage);
            jMessage.clear();
        }
        //saveFile["messages"].push_back(jMessage);
        //jMessage.clear();
    }

    for (auto& character : characters) {
        json jDMessage;
        for (auto& charDMessage : character.dummyMessages) {
            jDMessage["from"] = character.name;
            jDMessage["content"] = charDMessage;
            saveFile["dummyMessages"].push_back(jDMessage);
            jDMessage.clear();
        }
    }

    std::ofstream file("Content/Data/messages.json");
    if (file.is_open()) {
        file << std::setw(4) << saveFile << std::endl;
    }
    else {
        std::cout << "Failed to open file for writing: check directory Content/Data/ exists" << std::endl;
    }

    //file << std::setw(4) << saveFile << std::endl;

}