#pragma once

// Windows dependencies
#include <windows.h>
#include <winnls.h>

// Std dependencies
#include <string>

// Third party dependencies
#include <opencv2/core.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <glad/glad.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

template <typename TType>
inline const TType& min(const TType& a, const TType& b)
{
	return a < b ? a : b;
}

template <typename TType>
inline const TType& max(const TType& a, const TType& b)
{
	return a > b ? a : b;
}

// Function to convert wide string to UTF-8
inline std::string WideStringToUTF8(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL) + 1;
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

// Function to convert UTF-8 to wide string
inline std::wstring UTF8ToWideString(const std::string& str)
{
	if (str.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0) + 1;
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

inline void drawWindowTitle(const char* title, ImVec2 windowPos)
{
	// Hold cursor pos to restore later
	ImVec2 cursorPos = ImGui::GetCursorScreenPos();

	ImVec2 textPos = ImVec2(windowPos.x + 5, windowPos.y - 6);

	// Get the draw list and draw the border around the text
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 textSize = ImGui::CalcTextSize(title);
	ImVec2 borderMin = ImVec2(textPos.x - 3, textPos.y - 2);
	ImVec2 borderMax = ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2);
	drawList->AddRectFilled(borderMin, borderMax, ImGui::GetColorU32(ImGuiCol_TitleBg));
	drawList->AddRect(borderMin, borderMax, ImGui::GetColorU32(ImGuiCol_Border));

	// Draw text with border on top of the child window
	ImGui::SetCursorScreenPos(textPos);
	ImGui::TextUnformatted(title);

	// Restore cursor pos
	ImGui::SetCursorScreenPos(cursorPos);
}

inline void drawScreenView(cv::Mat& _frame, uint32_t _frameTexId)
{
	// Get available size
	ImVec2 availableSize = ImGui::GetContentRegionAvail();

	// Calculate aspect ratio
	float aspectRatio = _frame.cols / (float)_frame.rows;
	float displayWidth = availableSize.x;
	float displayHeight = availableSize.y;

	if (displayWidth / aspectRatio <= displayHeight)
	{
		displayHeight = displayWidth / aspectRatio;
	} else {
		displayWidth = displayHeight * aspectRatio;
	}

	// Compute padding
	float verPadding = (availableSize.y - displayHeight - ImGui::GetStyle().WindowPadding.y) / 2;
	float horPadding = (availableSize.x - displayWidth - ImGui::GetStyle().WindowPadding.x) / 2;
	ImGui::Dummy({0, verPadding});

	ImGui::Dummy({horPadding, 0});
	ImGui::SameLine();

	// Upload the image data to the texture
	glBindTexture(GL_TEXTURE_2D, _frameTexId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _frame.cols, _frame.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, _frame.data);
	ImGui::Image((void*)(intptr_t)_frameTexId, ImVec2(displayWidth, displayHeight));

	// Ensures we cover any remaining space
	ImGui::Dummy({0, verPadding});
}

inline void drawHorizontalSeparator(float& prevHeight, float& nextHeight, float availableSize, float minPrevHeight = 0, float minNextHeight = 0, float padding = 6.0f, const std::string separatorText = "")
{
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	bool done = false;
	float delta = 0.0f;
	ImGui::SetNextItemAllowOverlap();
	ImVec2 curCursorPos = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton(("##" + separatorText + "_separator").c_str(), ImVec2(-1, padding));
	if (ImGui::IsItemActive())
	{
		delta = ImGui::GetIO().MouseDelta.y;
		done = true;
	}

	// Visible separator
	bool hovered = ImGui::IsItemHovered();
	if (hovered)
    {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        ImGui::PushStyleColor(ImGuiCol_Separator, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
    }
	ImVec2 lastCursorPos = ImGui::GetCursorScreenPos();
	if (separatorText.size() > 0)
	{
		ImGui::SetCursorScreenPos({curCursorPos.x, curCursorPos.y - padding / 2});
		ImGui::SeparatorText(separatorText.c_str());
	}
	else
	{
		ImGui::SetCursorScreenPos({curCursorPos.x, curCursorPos.y});
		ImGui::Separator();
	}
	ImGui::SetCursorScreenPos(lastCursorPos);
	if (hovered)
	{
		ImGui::PopStyleColor(); // Reset the hover color
	}

	if (!done)
	{
		delta = 0;
	}
	float availableHeight = ImGui::GetContentRegionAvail().y;
	prevHeight = max(minPrevHeight, min(availableSize - minNextHeight, prevHeight + delta));
	nextHeight = max(minNextHeight, min(availableHeight, nextHeight - delta));
	ImGui::PopStyleVar();
}

// Function to convert HSV to RGB
inline void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b)
{
    int i = static_cast<int>(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6)
    {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    case 5: r = v, g = p, b = q; break;
    }
}

// Function to generate a random color using the golden ratio
inline cv::Scalar generateRandomColor()
{
    static float goldenRatioConjugate = 0.61803398875f;
    static float hue = static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Random initial hue

    hue = fmod(hue + goldenRatioConjugate, 1.0f); // Ensure hue stays within [0, 1]
    float sat = 0.5 + fmod(hue + goldenRatioConjugate, 0.5f); // Ensure hue stays within [0.5, 1]
    float val = 0.7 + fmod(sat + goldenRatioConjugate, 0.3f); // Ensure hue stays within [0.7, 1]

    float r, g, b;
    HSVtoRGB(hue, sat, val, r, g, b); // Convert HSV to RGB

    return { r * 255, g * 255, b * 255, 255.0 };
}

template<typename TType>
bool binarySearch(TType* arr, int l, int r, TType x, int& outIndex)
{
	if (r >= l)
	{
		int mid = l + (r - l) / 2;

		// If the element is present at the middle itself
		if (arr[mid] == x)
		{
			outIndex = mid;
			return true;
		}

		// If element is smaller than mid, then it can only be present in left subarray
		if (arr[mid] > x)
			return binarySearch(arr, l, mid - 1, x);

		// Else the element can only be present in right subarray
		return binarySearch(arr, mid + 1, r, x);
	}

	// We reach here when the element is not present in array
	outIndex = l;
	return false;
}

template<typename TType>
bool binarySearch(TType* arr, int size, TType x)
{
	return binarySearch(arr, 0, size - 1, x);
}

class ImGuiPanelGuard
{
  public:
	ImGuiPanelGuard(const char* strId, const ImVec2& size = ImVec2(0, 0), ImGuiChildFlags childFlags = ImGuiChildFlags_Border,
					ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
		: _strId(strId)
	{
		ImGui::Dummy({0, 3}); // Padding for title box
		ImGui::BeginChild(strId, size, childFlags, windowFlags);
		_windowPos = ImGui::GetWindowPos();
		ImGui::Dummy({0, 1}); // Padding for title box
	}

	~ImGuiPanelGuard()
	{
		ImGui::EndChild();
		drawWindowTitle(_strId, _windowPos);
	}

  private:
	ImVec2 _windowPos;
	const char* _strId;
};