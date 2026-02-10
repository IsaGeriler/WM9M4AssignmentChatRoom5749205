// Dear ImGui: standalone example application for Windows API + DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "sound.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <d3d11.h>
#include <tchar.h>

#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- CLIENT CODES FOR CHAT ROOM ---
constexpr unsigned int DEFAULT_BUFFER_SIZE = 1024;  // Default Buffer Size - 1024 Bytes
std::atomic<bool> isRunning = true;                 // Storing as atomic to prevent race conditions
std::atomic<bool> threadStarted = false;            // Storing as atomic to prevent race conditions
std::mutex mtx_client;
std::mutex mtx_client_list;

// Store the "usernames - chats" as a pair, in a map (first element of the list being "Broadcast" chat)
std::map<std::string, std::vector<std::string>> allChatsHistory;
std::set<std::string> activeChats;
static std::vector<std::string> activeClients;

// Views the current chat; default is set as "Broadcast"
std::string currentChat = "Broadcast";
std::string current_client = "";  // Client of the launched instance

// Receive Loop
static void Receive(SOCKET client_socket)
{
    Sound sound;
    while (isRunning)
    {
        // Receive the response from the server
        char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
        int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);

        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0'; // Null-terminate the received data
            
            std::stringstream stream_from_server(buffer);
            std::string current_line;

            while (std::getline(stream_from_server, current_line, '\n'))
            {
                if (current_line.empty()) continue;
                std::stringstream line_ss(current_line);

                std::string command = "";
                std::string sender_username = "";
                std::string target_username = "";  // for DirectMessage only
                std::string message = "";

                // buffer = [Handshake/MessageType] [ClientName] [MessageBody]
                line_ss >> command;          // Gets the handshake protocol/type of message came
                line_ss >> sender_username;  // Gets the client name where the message came from

                if (buffer == "/exit") isRunning = false; // isRunning.store(false, std::memory_order_relaxed);

                std::cout << command << std::endl;
                std::cout << sender_username << std::endl;
                std::cout << (int)sender_username.back() << std::endl;
                // std::cout << (strcmp(command.c_str(), "[SERVER]")) << '\n';

                if (strcmp(command.c_str(), "[SERVER]") == 0)
                {
                    std::lock_guard<std::mutex> lock(mtx_client);
                    std::ws(line_ss);
                    std::getline(line_ss, message);  // Gets the rest of the message body
                    message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
                    std::cout << message << '\n';

                    if (strcmp(message.c_str(), "joined the chat") == 0)
                    {
                        auto iter = std::find(activeClients.begin(), activeClients.end(), sender_username);
                        if (iter == activeClients.end()) activeClients.emplace_back(sender_username);
                        allChatsHistory["Broadcast"].emplace_back(buffer);
                    }

                    else if (strcmp(message.c_str(), "left the chat") == 0)
                    {
                        auto iter = std::find(activeClients.begin(), activeClients.end(), sender_username);
                        if (iter != activeClients.end()) activeClients.erase(iter);
                        allChatsHistory["Broadcast"].emplace_back(buffer);
                    }

                    else if (strcmp(message.c_str(), "is not connected!") == 0)
                    {
                        allChatsHistory[sender_username].emplace_back(buffer);
                    }
                    //sound.playServerSound();
                }
                else if (strcmp(command.c_str(), "[DirectMessage]") == 0)
                {
                    std::lock_guard<std::mutex> lock(mtx_client);
                    line_ss >> target_username;
                    std::ws(line_ss);
                    std::getline(line_ss, message);  // Gets the rest of the message body
                    std::string finalMessage = sender_username + ": " + message;
                    allChatsHistory[sender_username].emplace_back(finalMessage);
                    allChatsHistory[target_username].emplace_back(finalMessage);
                    //sound.playDmSound();
                }
                else if (strcmp(command.c_str(), "[BroadcastMessage]") == 0)
                {
                    std::lock_guard<std::mutex> lock(mtx_client);
                    std::ws(line_ss);
                    std::getline(line_ss, message);  // Gets the rest of the message body
                    std::string finalMessage = sender_username + ": " + message;
                    allChatsHistory["Broadcast"].emplace_back(finalMessage);
                    //sound.playBroadcastSound();
                }
            }
        }
        else if (bytes_received == 0)
        {
            std::cout << "Connection closed by server." << std::endl;
            isRunning = false;
        }
        else
        {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
            isRunning = false;
        }
    }
}
// --- CLIENT CODES FOR CHAT ROOM END ---

// Main code
int main(int, char**)
{
    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale better, consider using the 'ProggyVector' from the same author!
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // States
    std::atomic<bool> done = false;
    std::atomic<bool> isNotConnected = true;

    std::string login_status = "";

    bool show_login_window = true;
    bool show_chat_window = true;
    bool show_private_window = true;

    bool login_button_clicked = false;
    bool send_button_clicked = false;
    bool send_private_button_clicked = false;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Username and Message Buffers
    char usernameBuffer[32]{};
    char messageBuffer[256]{};
    char privateMessageBuffer[256]{};

    // Client codes before the main loop start here
    const char* host = "127.0.0.1";
    unsigned int port = 65432;
    std::string message = "Hello, server!";

    // Initialise WinSock Library
    // - Version Requested as a Word: 2.2
    // - Pointer to a WSADATA structure
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Create a socket
    // - Address Family: IPv4 (AF_INET)
    // - Socket Type: TCP (SOCK_STREAM)
    // - Protocol: TCP (IPPROTO_TCP)
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Convert an IP address from string to binary
    // - Address Family: IPv4 (AF_INET)
    // - Source IP String: host ("127.0.0.1")
    // - Destination Pointer: Sturcture holding binary representation
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0)
    {
        std::cerr << "Invalid address/Address not supported" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    // Establishing the connection
    // - s: The socket descriptor
    // - name: Pointer to a sockaddr structure (containing the server's address and port)
    // - namelen: Size of the sockaddr structure
    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR)
    {
        std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    std::cout << "Connected to the server!" << std::endl;
    // Client codes before the main loop end here

    // Main loop
    while (!done.load(std::memory_order_relaxed))  // !done
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done.store(true, std::memory_order_relaxed);  // done = true;
        }
        if (done.load(std::memory_order_relaxed))  // done
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // DearImGui Codes for ChatRoom Start From Here
        // User has not logged in to the server
        if (isNotConnected.load(std::memory_order_relaxed))
        {
            ImGui::SetNextWindowSize(ImVec2(800, 150));
            ImGui::SetWindowSize(ImVec2(100, 100));

            // Login Window
            ImGui::Begin("Login", &show_login_window);

            // Textbox to type username in the window
            ImGui::Text("Please Enter Your Username");
            bool username_entered = ImGui::InputText("##Username: ", usernameBuffer, sizeof(usernameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();

            ImGui::Text("Username");

            ImGui::SameLine();

            // Button, starting state as non-pressable when there's an empty string
            ImGui::BeginDisabled(!strcmp(usernameBuffer, ""));
            if (ImGui::Button("Login")) login_button_clicked = !login_button_clicked;
            ImGui::EndDisabled();

            ImGui::Text(login_status.c_str());
            std::string str(usernameBuffer);

            if (login_button_clicked || username_entered)
            {
                if (str.find(' ') != std::string::npos)
                {
                    login_status = "Username cannot contain white spaces!";
                    login_button_clicked = false;
                }
                else
                {
                    if (send(client_socket, usernameBuffer, sizeof(usernameBuffer), 0) == SOCKET_ERROR)
                    {
                        login_status = "Send failed with error: " + WSAGetLastError() + '\n';
                        closesocket(client_socket);
                        WSACleanup();
                        return 1;
                    }
                    // Receive the "UNIQUE" or "NOT_UNIQUE" from the server
                    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
                    int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);

                    if (bytes_received > 0)
                    {
                        buffer[bytes_received] = '\0';  // Null-terminate the received data
                        if (strcmp(buffer, "UNIQUE") == 0)
                        {
                            current_client = usernameBuffer;
                            isNotConnected.store(false, std::memory_order_relaxed);  // Look here...
                        }
                        else if (strcmp(buffer, "NOT_UNIQUE") == 0) login_status = "Username already taken, try again!";
                    }
                    else if (bytes_received == 0) login_status = "Connection closed by server.";
                    else login_status = "Receive failed with error: " + WSAGetLastError();
                    login_button_clicked = !login_button_clicked;
                }
            }
            ImGui::End();
        }
        // User has logged in to the server
        else
        {
            // Username taken, now we can launch the thread
            if (!threadStarted.load(std::memory_order_relaxed))
            {
                std::thread t(Receive, client_socket);
                t.detach();
                threadStarted.store(true, std::memory_order_relaxed);
            }

            // Is user still connected?
            if (!isRunning.load(std::memory_order_relaxed))
            {
                done.store(true, std::memory_order_relaxed);
            }

            ImGui::SetNextWindowSize(ImVec2(900, 615));
            ImGui::SetWindowSize(ImVec2(100, 100));

            if (show_chat_window)
            {
                ImGui::Begin("Chat Client", &show_chat_window);
                ImGui::BeginChild("Users", ImVec2(225, 500), true);
                ImGui::Text("Users:");
                ImGui::Separator();
                ImGui::BeginChild("UserList", ImVec2(0, 0), false);

                // List the users here... potentially make a request to server like "/list"
                // When clicked on a user, make sure to open another window for DMs

                mtx_client_list.lock();
                for (size_t i = 0; i < activeClients.size(); i++)
                {
                    // std::cout << activeClients[i].c_str() << '\n';
                    if (strcmp(current_client.c_str(), activeClients[i].c_str()) == 0) continue;
                    if (ImGui::Selectable(activeClients[i].c_str())) activeChats.insert(activeClients[i].c_str());
                }
                mtx_client_list.unlock();

                ImGui::EndChild();
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("Messages", ImVec2(635, 500), true);
                ImGui::SetScrollHereY(1.f);

                // List the broadcasting message here...
                // Nice to have: Give each user an unique color to distinguish each other

                for (size_t i = 0; i < allChatsHistory["Broadcast"].size(); i++)
                {
                    ImGui::TextWrapped("%s", allChatsHistory["Broadcast"][i].c_str());
                }

                ImGui::EndChild();

                bool broadcast_sent = ImGui::InputText("##Message: ", messageBuffer, sizeof(messageBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::SameLine();
                ImGui::Text("Message");

                ImGui::SameLine();

                // Button, starting state as non-pressable when there's an empty string
                ImGui::BeginDisabled(!strcmp(messageBuffer, ""));
                if (ImGui::Button("Send")) send_button_clicked = !send_button_clicked;
                ImGui::EndDisabled();

                if (send_button_clicked || broadcast_sent)
                {
                    // ImGui::Text("This is only for debugging... send logic will go here...");
                    // Send the message to the server
                    if (send(client_socket, messageBuffer, sizeof(messageBuffer), 0) == SOCKET_ERROR)
                    {
                        std::cerr << "Send failed with error: " + WSAGetLastError() + '\n';
                        return 1;
                        closesocket(client_socket);
                        WSACleanup();
                    }
                    memset(messageBuffer, '\0', sizeof(messageBuffer));  // Clear the message buffer after everything
                    send_button_clicked = false;
                }

                ImGui::SetNextWindowSize(ImVec2(1000, 400));
                ImGui::SetWindowSize(ImVec2(100, 100));

                std::lock_guard<std::mutex> lock(mtx_client_list);
                for (auto const& client : activeChats)
                {
                    std::string window_name = "Private Chat with " + client;
                    if (ImGui::Begin(window_name.c_str(), &show_private_window))
                    {
                        ImGui::BeginChild("PrivateMessage", ImVec2(975, 285), true);
                        ImGui::SetScrollHereY(1.f);

                        // List the direct/private messages here... 
                        for (size_t i = 0; i < allChatsHistory[client].size(); i++)
                        {
                            ImGui::TextWrapped("%s", allChatsHistory[client][i].c_str());
                        }

                        ImGui::EndChild();

                        bool direct_sent = ImGui::InputText("##PrivateMessage: ", privateMessageBuffer, sizeof(privateMessageBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

                        ImGui::SameLine();
                        ImGui::Text("Private Message");

                        ImGui::SameLine();

                        // Button, starting state as non-pressable when there's an empty string
                        ImGui::BeginDisabled(!strcmp(privateMessageBuffer, ""));
                        if (ImGui::Button("Send Private")) send_private_button_clicked = !send_private_button_clicked;
                        ImGui::EndDisabled();

                        if (send_private_button_clicked || direct_sent)
                        {
                            // ImGui::Text("This is only for debugging... send logic will go here...");
                            // Construct the message buffer to a string that the server can understand
                            std::string message(privateMessageBuffer);
                            std::string finalMessage = "/dm " + client + " " + message;

                            if (send(client_socket, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0) == SOCKET_ERROR)
                            {
                                std::cerr << "Send failed with error: " + WSAGetLastError() + '\n';
                                return 1;
                                closesocket(client_socket);
                                WSACleanup();
                            }
                            // Clear the message buffer after everything and set button state to false
                            memset(privateMessageBuffer, '\0', sizeof(privateMessageBuffer));
                            send_private_button_clicked = false;
                        }
                    }
                    ImGui::End();
                }
                ImGui::End();
            }
            else
            {
                // User pressed 'X' at the chat client; disconnect them!
                if (send(client_socket, "/exit", sizeof("/exit"), 0) == SOCKET_ERROR)
                {
                    std::cerr << "Send failed with error: " + WSAGetLastError() + '\n';
                    return 1;
                    closesocket(client_socket);
                    WSACleanup();
                }
            }
        }
        // DearImGui Codes for ChatRoom End Here

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    closesocket(client_socket);
    WSACleanup();

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
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