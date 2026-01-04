const std = @import("std");
const version = @import("build.zig.zon").version;
const zcc = @import("compile_commands");

const OPENRBRVR_VERSION = .{
    .openRBRVR_Major = "2",
    .openRBRVR_Minor = "2",
    .openRBRVR_Patch = "1",
    .openRBRVR_Tweak = "0",
    .openRBRVR_TweakStr = "-horizonlock",
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

    // For compile_commands.json
    var targets: std.ArrayListUnmanaged(*std.Build.Step.Compile) = .empty;
    targets.append(b.allocator, dll) catch @panic("OOM");
    _ = zcc.createStep(b, "cdb", targets.toOwnedSlice(b.allocator) catch @panic("OOM"));
}
