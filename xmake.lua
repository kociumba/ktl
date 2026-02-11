add_rules("mode.debug", "mode.release")

set_languages("cxx23")

set_policy("run.autobuild", true)
set_policy("check.auto_map_flags", true)
set_policy("build.progress_style", "multirow")

-- if is_mode("debug") then
    add_requires("catch2 3.x", {optional = true})
-- end

target("ktl")
    set_kind("headeronly")
    add_headerfiles("src/**.h", {public = true})
    add_includedirs("src/", {public = true})
    add_extrafiles(".clang-format")

for _, file in ipairs(os.files("examples/*.cpp")) do
    local name = path.basename(file)

    target(name)
        set_kind("binary")
        set_group("examples")
        set_default(false)
        add_files(file)
        add_headerfiles("examples/**.h")
        add_deps("ktl")

        set_warnings("all", "extra")

        add_cxflags("/permissive-", "/analyze", "/GS")

        local project_root = os.projectdir():gsub("\\", "/")
        add_defines("PROJECT_ROOT=\"" .. project_root .. "/\"")
end

-- if is_mode("debug") then
    target("ktl_tests")
        set_kind("binary")
        set_group("tests")
        set_default(true)
        add_files("tests/*.cpp")
        add_headerfiles("tests/**.h")
        add_deps("ktl")
        add_packages("catch2")

        set_warnings("all", "extra")

        add_cxflags("/permissive-", "/analyze", "/GS")

        local project_root = os.projectdir():gsub("\\", "/")
        add_defines("PROJECT_ROOT=\"" .. project_root .. "/\"")

        -- Register as test runner
        add_tests("default", {
            rundir = os.projectdir(),
            runargs = {"--reporter", "console"}
        })
-- end