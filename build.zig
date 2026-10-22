const std = @import("std");
const version = @import("build.zig.zon").version;
const zcc = @import("compile_commands");

const OPENRBRVR_VERSION = .{
    .openRBRVR_Major = "2",
    .openRBRVR_Minor = "3",
    .openRBRVR_Patch = "0",
    .openRBRVR_Tweak = "0",
    .openRBRVR_TweakStr = "-beta.1",
};

pub fn build(b: *std.Build) void {
    const supported_targets = &.{
        std.Target.Query{ .cpu_arch = .x86, .os_tag = .windows, .abi = .msvc },
    };
    const target = b.standardTargetOptions(.{
        .default_target = supported_targets[0],
        .whitelist = supported_targets,
    });
    const optimize = b.standardOptimizeOption(.{});

    const dll = b.addLibrary(.{
        .name = "openRBRVR",
        .linkage = .dynamic,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    dll.linkLibC();
    dll.addCSourceFiles(.{ .files = &.{
        "src/API.cpp",
        "src/Dx.cpp",
        "src/Globals.cpp",
        "src/HandAssets.cpp",
        "src/HandMesh.cpp",
        "src/HandTracking.cpp",
        "src/Menu.cpp",
        "src/OpenVR.cpp",
        "src/OpenXR.cpp",
        "src/RBR.cpp",
        "src/RenderTarget.cpp",
        "src/VR.cpp",
        "src/Vertex.cpp",
        "src/Util.cpp",
        "src/openRBRVR.cpp",
    }, .flags = &.{
        "-Wno-ignored-attributes",
        "--std=c++23",
    } });

    dll.addLibraryPath(.{ .cwd_relative = "thirdparty/lib" });

    dll.addIncludePath(.{ .cwd_relative = "thirdparty" });
    dll.addIncludePath(.{ .cwd_relative = "thirdparty/dxvk/include/vulkan/include" });
    dll.addIncludePath(.{ .cwd_relative = "thirdparty/dxvk/src/d3d9" });
    dll.addIncludePath(.{ .cwd_relative = "thirdparty/glm" });
    dll.addIncludePath(.{ .cwd_relative = "thirdparty/minhook/include" });
    dll.addIncludePath(.{ .cwd_relative = "thirdparty/openvr" });
    dll.addIncludePath(.{ .cwd_relative = "thirdparty/openxr" });

    const versionHeader = b.addConfigHeader(
        .{
            .style = .{ .cmake = b.path("src/Version.hpp.in") },
            .include_path = "Version.hpp",
        },
        OPENRBRVR_VERSION,
    );

    const resourceFile = b.addConfigHeader(
        .{
            .style = .{ .cmake = b.path("src/Version.rc.in") },
            .include_path = "Version.rc",
        },
        OPENRBRVR_VERSION,
    );

    dll.addConfigHeader(versionHeader);
    dll.addWin32ResourceFile(.{ .file = resourceFile.getOutput() });

    dll.linkSystemLibrary("advapi32");
    dll.linkSystemLibrary("d3d11");
    dll.linkSystemLibrary("d3d9");
    dll.linkSystemLibrary("dxgi");
    dll.linkSystemLibrary("libminhook.x86");
    dll.linkSystemLibrary("openvr_api");
    dll.linkSystemLibrary("openxr_loader");
    dll.linkSystemLibrary("user32");
    dll.linkSystemLibrary("version");

    b.installArtifact(dll);
    b.installFile("assets/valve_hands/LICENSE", "bin/Valve-hand-models-LICENSE.txt");

    const hand_tests = b.addExecutable(.{
        .name = "hand-tests",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    hand_tests.linkLibC();
    hand_tests.linkSystemLibrary("user32");
    hand_tests.addIncludePath(b.path("src"));
    hand_tests.addIncludePath(b.path("thirdparty"));
    hand_tests.addIncludePath(b.path("thirdparty/glm"));
    hand_tests.addIncludePath(b.path("thirdparty/openxr"));
    hand_tests.addIncludePath(b.path("thirdparty/openvr"));
    hand_tests.addIncludePath(b.path("thirdparty/dxvk/src/d3d9"));
    hand_tests.addIncludePath(b.path("thirdparty/dxvk/include/vulkan/include"));
    hand_tests.addCSourceFiles(.{
        .files = &.{ "tests/Hands.cpp", "src/HandTracking.cpp", "src/HandMesh.cpp", "src/HandAssets.cpp" },
        .flags = &.{ "-Wno-ignored-attributes", "-Wno-deprecated-literal-operator", "-Wno-unused-command-line-argument", "--std=c++23" },
    });
    const test_step = b.step("test", "Test hand tracking lifecycle, visibility, mesh and configuration");
    test_step.dependOn(&b.addRunArtifact(hand_tests).step);

    // Optional real D3D9 smoke test: renders the embedded assets without RBR or
    // a headset, including bind poses, palms and curled fingers.
    const preview = b.addExecutable(.{
        .name = "hand-preview",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });
    preview.linkLibC();
    preview.linkSystemLibrary("user32");
    preview.linkSystemLibrary("d3d9");
    preview.addIncludePath(b.path("src"));
    preview.addIncludePath(b.path("thirdparty"));
    preview.addIncludePath(b.path("thirdparty/glm"));
    preview.addIncludePath(b.path("thirdparty/openxr"));
    preview.addCSourceFiles(.{
        .files = &.{ "tools/HandPreview.cpp", "src/HandAssets.cpp", "src/HandMesh.cpp" },
        .flags = &.{ "-Wno-ignored-attributes", "--std=c++23" },
    });
    const preview_run = b.addRunArtifact(preview);
    if (b.args) |args| preview_run.addArgs(args);
    b.step("hand-preview", "Render Valve gloves with D3D9 to a BMP without a headset").dependOn(&preview_run.step);

    // For compile_commands.json
    var targets: std.ArrayListUnmanaged(*std.Build.Step.Compile) = .empty;
    targets.append(b.allocator, dll) catch @panic("OOM");
    _ = zcc.createStep(b, "cdb", targets.toOwnedSlice(b.allocator) catch @panic("OOM"));
}
