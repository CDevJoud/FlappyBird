#include <Windows.h>
#include <vector>
#include <time.h>
#include <assert.h>
#include <cmath>
#include <string>

class Console {
public:
	Console(WORD width, WORD height, BYTE pw, BYTE ph) {
		m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		if (m_hConsole == nullptr) {
			AllocConsole();
			m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			assert((m_hConsole != nullptr) && "Could Not Create Console Window!");
		}

		SMALL_RECT rect = { 0, 0, 1, 1 };
		SetConsoleWindowInfo(this->m_hConsole, TRUE, &rect);
		if (!SetConsoleScreenBufferSize(this->m_hConsole, { (SHORT)(width), (SHORT)(height) })) {
			MessageBoxA(nullptr, "Couldn't set the console screen buffer size!", "Error", MB_ICONERROR | MB_OK);
		}

		SetConsoleActiveScreenBuffer(this->m_hConsole);

		CONSOLE_FONT_INFOEX cfi;

		cfi.cbSize = sizeof(cfi);
		cfi.nFont = 0;
		cfi.dwFontSize.X = pw;
		cfi.dwFontSize.Y = ph;
		cfi.FontFamily = FF_DONTCARE;
		cfi.FontWeight = FW_NORMAL;

		wcscpy_s(cfi.FaceName, L"Consolas");
		SetCurrentConsoleFontEx(this->m_hConsole, FALSE, &cfi);
		viewport.Left = 0;
		viewport.Top = 0;
		viewport.Right = width - 1;
		viewport.Bottom = height - 1;

		buffer = new CHAR_INFO[width * height]{};

		SetConsoleWindowInfo(this->m_hConsole, TRUE, &viewport);

		this->m_bIsOpen = bool(this->m_hConsole);
	}
	virtual ~Console() {
		FreeConsole();
		delete[] buffer;
	}
	HANDLE GetNativeHandle() const {
		return this->m_hConsole;
	}
	SMALL_RECT GetViewport() const {
		return this->viewport;
	}
	PCHAR_INFO GetScreenBuffer() const {
		return this->buffer;
	}
	VOID Display() {
		COORD size = { viewport.Right - viewport.Left + 1, viewport.Bottom - viewport.Top + 1 };
		WriteConsoleOutputW(this->m_hConsole, buffer, size, {}, &viewport);
	}
	bool IsOpen() const {
		return this->m_bIsOpen;
	}
	VOID Close() {
		this->m_bIsOpen = false;
	}
private:
	PCHAR_INFO buffer = nullptr;
	SMALL_RECT viewport = {};
	HANDLE m_hConsole = INVALID_HANDLE_VALUE;
	bool m_bIsOpen = false;
};

class Renderer {
public:
	Renderer() = default;
	Renderer(PCHAR_INFO buffer, SMALL_RECT viewport) : buffer(buffer), viewport(viewport) {
		
	}
	void InitRenderer(PCHAR_INFO buffer, SMALL_RECT viewport) {
		this->buffer = buffer;
		this->viewport = viewport;
	}
	
	void SetPixel(SHORT x, SHORT y, WORD pixel = 0x2588, BYTE color = 0xff) const {
		if (x >= viewport.Left && x < viewport.Right && y >= viewport.Top && y < viewport.Bottom) {
			this->buffer[y * (viewport.Right - viewport.Left + 1) + x].Char.UnicodeChar = pixel;
			this->buffer[y * (viewport.Right - viewport.Left + 1) + x].Attributes = color;
		}
	}

	void Fill(SHORT x1, SHORT y1, SHORT x2, SHORT y2, WORD pixel = 0x2588, BYTE color = 0xff) const {
		this->CalcClipOn(&x1, &y1);
		this->CalcClipOn(&x2, &y2);
		for (int h = y1; h < y2; h++) {
			for (int w = x1; w < x2; w++) {
				this->SetPixel(w, h, pixel, color);
			}
		}
	}
	void RenderSprite(LPCSTR txtSprite, SHORT x, SHORT y, WORD w, WORD h, BYTE dye) const {
		for (int i = y; i < y + h; i++) {
			for (int j = x; j < x + w; j++) {
				this->SetPixel(j, i, txtSprite[(i - y) * w + (j - x)], dye);
			}
		}
	}
	void CalcClipOn(PSHORT x, PSHORT y) const {
		if (*x < 0) *x = 0;
		if (*x > this->viewport.Right) *x = this->viewport.Right;
		if (*y < 0) *y = 0;
		if (*y > this->viewport.Bottom) *y = this->viewport.Bottom;
	}
	void RenderText(SHORT x, SHORT y, LPCSTR text, BYTE color = 0x0f) const {
		for (int i = 0; i < strlen(text); i++) {
			this->SetPixel(x + i, y, text[i], color);
		}
	}
private:
	PCHAR_INFO buffer;
	SMALL_RECT viewport;
};

class Timer {
public:
	Timer() {
		QueryPerformanceFrequency(&freq);

		QueryPerformanceCounter(&counter);
	}

	float GetDeltaTime() {
		LARGE_INTEGER currentTime{};
		QueryPerformanceCounter(&currentTime);

		LONGLONG timeDiff = currentTime.QuadPart - counter.QuadPart;

		float dt = (float)timeDiff / (float)freq.QuadPart;

		counter.QuadPart = currentTime.QuadPart;

		return dt;
	}
private:
	LARGE_INTEGER freq;
	LARGE_INTEGER counter;
};

class Game : private Renderer, private Console {
private:
	struct Pipes {
		SMALL_RECT pip1, pip2, gap;
	};
public:
	Game() :
		screen_width(120),
		screen_height(64),
		Console(120, 64, 8, 16) {
		Renderer::InitRenderer(Console::GetScreenBuffer(), Console::GetViewport());
		GeneratePipes(8, 10, 40);
		srand((unsigned int)time(0));
	}
	~Game() {

	}
	INT run() {
		posx = posy = 0.0f;
		posx = 10;
		Timer time;

		while (Console::IsOpen()) {
			float dt = time.GetDeltaTime();
			if(!this->m_bHalt) {
				this->timePassed += dt;
			}

			if (this->timePassed >= 15.0f) {
				this->GeneratePipes(10, 10, 40);
				this->timePassed = 0;
			}
			if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
				Console::Close();
			}

			Update(dt);
			Render();
			
			Sleep(16);
		}
		return 0; 
	}
private:
	void Update(const float dt) {

		velocity += gravity * dt;
		posy += velocity * dt; 

		if (velocity > 600.0f) velocity = 600.0f;

		if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
			this->state = true;
			velocity = jumpForce;
		}
		else {
			this->state = false;
		}
		if(!this->m_bHalt) {
			for (auto& i : this->pipes) {
				float moveStep = 10.0f * dt;
				i.pip1.Left -= moveStep;
				i.pip1.Right -= moveStep;

				i.pip2.Left -= moveStep;
				i.pip2.Right -= moveStep;

				i.gap.Left -= moveStep;
				i.gap.Right -= moveStep;

				if (CheckInBoundaries(posx, posy, i.pip1)) {
					m_bHalt = true;
				}
				else if (CheckInBoundaries(posx, posy, i.pip2)) {
					m_bHalt = true;
				}
				else if (posy > 64) {
					m_bHalt = true;
				}
				else if (CheckInBoundaries(posx, posy, i.gap)) {
					this->counts++;
				}
			}
		}
		else {
			if (GetAsyncKeyState(L'R') & 0x8000) {
				this->m_bHalt = false;
				this->posy = 10;
				this->velocity = 0;

				this->pipes.clear();
				this->pipes.shrink_to_fit();

				this->counts = 0;
				this->GeneratePipes(10, 10, 60); 
			}
		}
	}
	bool CheckInBoundaries(SHORT x, SHORT y, SMALL_RECT rect) {
		return (x >= rect.Left && x <= rect.Right &&
			y >= rect.Top && y <= rect.Bottom);
	}
	void Render() {
		Renderer::Fill(0, 0, 120, 64, 0x2588, 0x11);

		if (state) {
			Renderer::RenderSprite(flappyDown, posx, posy, 4, 3, 0x12);
		}
		else {
			Renderer::RenderSprite(flappyUp, posx, posy, 4, 3, 0x12);
		}
		for (auto& i : this->pipes) {
			Renderer::Fill(i.pip1.Left, i.pip1.Top, i.pip1.Right, i.pip1.Bottom, 0x2588, 0xaa);
			Renderer::Fill(i.pip2.Left, i.pip2.Top, i.pip2.Right, i.pip2.Bottom, 0x2588, 0xaa);
			//Renderer::Fill(i.gap.Left, i.gap.Top, i.gap.Right, i.gap.Bottom, 0x2588, 0x44);
		}
		std::string score = "Score: " + std::to_string(this->counts);
		Renderer::RenderText(10, 10, score.c_str());

		if (this->m_bHalt) {
			std::string msg = "You Lost! Press R To Play Again!";
			Renderer::RenderText(60 - msg.length() / 2, 32, msg.c_str(), 0x4f);
		}

		Console::Display();
	}
	
	void GeneratePipes(int amount, int gap_between_pipes, int distance_between_pipes, int minHeight = 10, int pipeWidth = 5) {
		int maxHeight = screen_height - gap_between_pipes - 5;


		for (int i = 0; i < amount; i++) {
			int topPipeHeight = minHeight + rand() % (maxHeight - minHeight);
			int bottomPipeHeight = screen_height - topPipeHeight - gap_between_pipes;

			Pipes newPipes;
			int pipeX = screen_width + (i * distance_between_pipes);

			newPipes.pip1.Left = pipeX;
			newPipes.pip1.Top = 0;
			newPipes.pip1.Right = newPipes.pip1.Left + pipeWidth;
			newPipes.pip1.Bottom = topPipeHeight;

			newPipes.pip2.Left = pipeX;
			newPipes.pip2.Top = screen_height - bottomPipeHeight;
			newPipes.pip2.Right = newPipes.pip2.Left + pipeWidth;
			newPipes.pip2.Bottom = screen_height;

			newPipes.gap.Left = newPipes.pip1.Left;
			newPipes.gap.Top = newPipes.pip1.Bottom;
			newPipes.gap.Right = newPipes.pip2.Left + pipeWidth;
			newPipes.gap.Bottom = newPipes.pip2.Top;

			pipes.push_back(newPipes);
		}
	}
	std::vector<Pipes> pipes;
	LPCSTR flappyUp = R"( (O>\@@/ ^^ )";
	LPCSTR flappyDown = " (O>/@@\\ ^^ ";
	float posx, posy; 
	bool state = false; 
	int screen_width = 120; 
	int screen_height = 64; 
	float timePassed = 0.0f;

	float velocity = 0; 
	const float gravity = 100.82f; 
	const float jumpForce = -25.0f; 
	int counts = 0.0f;
	bool m_bHalt = false;
};

INT WINAPI ::wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPTSTR, _In_ INT)
{
	Game game;
	ExitProcess(game.run());
}
