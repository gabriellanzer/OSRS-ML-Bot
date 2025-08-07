add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate")

add_rules("mode.debug", "mode.release", "mode.releasedbg")


-- UI requirements
add_requires("imgui v1.91.0-docking", {configs = {opengl3 = true, glfw = true}, system = false})
add_requires("stb", {configs = {headeronly = true}})
add_requires("glfw", "glm", "glad", "fmt", "nativefiledialog-extended")

-- Bot requirements
add_requires("onnxruntime", {configs = {gpu = true}})
add_requires("opencv", {configs = {avif = false, shared = false}, system = false})
add_requires("nlohmann_json")

target("osrs-bot")
    set_kind("binary")

	-- UI packages
	add_packages("imgui", "stb", "glfw", "glm", "glad", "fmt", "nativefiledialog-extended")

	-- Bot packages
	add_packages("onnxruntime", "opencv", "nlohmann_json")

	add_includedirs("src", "include")
	add_headerfiles("src/**.h", "include/**.h")

    add_files("src/**.cpp")

	set_languages("c++20")

	if is_mode("debug") then
		add_defines("DEBUG_BUILD");
		set_targetdir("bin/Debug/")
	elseif is_mode("release") or is_mode("releasedbg") then
		add_defines("RELEASE_BUILD");
		set_targetdir("bin/Release/")
	end

	-- Copy the ONNX Runtime DLL to the target directory after build
    after_build(function (target)
		print("Build finished! Binary files written to " .. target:targetdir())

		print("\n")
		print("Copying ONNX Runtime DLLs to target directory (required so it doesn't pick system dlls)")
        local onnxruntime_dir = path.join(target:pkg("onnxruntime"):installdir(), "bin")
        local dll_files = os.files(path.join(onnxruntime_dir, "*.dll"))
        for _, dll in ipairs(dll_files) do
			print("Copying " .. dll)
            os.cp(dll, target:targetdir())
        end
		print("Done!\n")

		-- Read CUDA_PATH from environment variable
		print("Searching for CUDA 12 path")
		local cuda_path_env = os.getenv("CUDA_PATH")
		if not cuda_path_env then
			raise("CUDA_PATH environment variable is not set! Please install CUDA 12 and try again. You might have to run 'xmake f -c' to clear the cache.")
		end
		
		-- Split CUDA_PATH by semicolon in case there are multiple paths
		local cuda_paths = {}
		for path in cuda_path_env:gmatch("[^;]+") do
			table.insert(cuda_paths, path)
		end
		
		print("Found " .. #cuda_paths .. " CUDA path(s):")
		for i, cuda_dir in ipairs(cuda_paths) do
			print("  " .. i .. ": " .. cuda_dir)
		end
		
		-- Try each path until we find one with the required DLLs
		local cuda_path = nil
		for _, cuda_dir in ipairs(cuda_paths) do
			local cudartPath = path.join(cuda_dir, "bin/cudart64_12.dll")
			local cudnnPath = path.join(cuda_dir, "bin/cudnn64_8.dll")
			if os.isfile(cudartPath) and os.isfile(cudnnPath) then
				cuda_path = cuda_dir
				print("Using CUDA path: " .. cuda_path)
				break
			end
		end
		
		if not cuda_path then
			raise("No valid CUDA path found with required DLLs! Please install CUDA 12 and cuDNN 8, and try again. You might have to run 'xmake f -c' to clear the cache.")
		end

		local cudartPath = path.join(cuda_path, "bin/cudart64_12.dll")
		local cudnnPath = path.join(cuda_path, "bin/cudnn64_8.dll")

		-- Copy icon from resources to target directory
		if not os.isfile(path.join(target:targetdir(), "icon.png")) then
			print("Copying icon to target directory")
			os.cp("rsc/icon.png", target:targetdir())
		end
    end)
target_end()