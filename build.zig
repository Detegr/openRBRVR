const std = @import("std");
const version = @import("build.zig.zon").version;
const zcc = @import("compile_commands");

const OPENRBRVR_VERSION = .{
    .openRBRVR_Major = "2",
    .openRBRVR_Minor = "3",
    .openRBRVR_Patch = "0",
    .openRBRVR_Tweak = "0",
    .openRBRVR_TweakStr = "-beta",
};

pub fn build(b: *std.Build) void {
    const supported_targets = &.{
        std.Target.Query{ .cpu_arch = .x86, .os_tag = .windows, .abi = .gnu },
    };
    const target = b.standardTargetOptions(.{
        .default_target = supported_targets[0],
        .whitelist = supported_targets,
    });
    const optimize = b.standardOptimizeOption(.{});

    const minhook = b.dependency("minhook", .{
        .target = target,
        .optimize = .ReleaseFast,
    }).module("minhook");

    const dxvk = b.dependency("dxvk", .{
        .target = target,
        .optimize = optimize,
    });
    const d3d9 = dxvk.artifact("d3d9");

    var mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
        .sanitize_c = .off,
    });

    mod.addImport("minhook", minhook);
    for (minhook.include_dirs.items) |dir| {
        mod.addIncludePath(dir.path);
    }

    mod.addCSourceFiles(.{ .files = &.{
        "src/API.cpp",
        "src/DummyVR.cpp",
        "src/Dx.cpp",
        "src/Globals.cpp",
        "src/HandAssets.cpp",
        "src/HandMesh.cpp",
        "src/HandTracking.cpp",
        "src/HedgeHog.cpp",
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

    mod.addIncludePath(.{ .cwd_relative = "thirdparty" });
    mod.addIncludePath(.{ .cwd_relative = "thirdparty/glm" });
    mod.addIncludePath(.{ .cwd_relative = "thirdparty/openvr" });
    mod.addIncludePath(.{ .cwd_relative = "thirdparty/openxr" });
    mod.addIncludePath(.{ .dependency = .{ .dependency = dxvk, .sub_path = "include" } });

    const dll = b.addLibrary(.{
        .name = "openRBRVR",
        .linkage = .dynamic,
        .root_module = mod,
    });

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

    mod.addLibraryPath(b.path("thirdparty/dll"));

    mod.addConfigHeader(versionHeader);
    mod.addWin32ResourceFile(.{ .file = resourceFile.getOutputFile() });
    mod.linkLibrary(d3d9);

    mod.linkSystemLibrary("advapi32", .{});
    mod.linkSystemLibrary("d3d11", .{});
    mod.linkSystemLibrary("d3d9", .{});
    mod.linkSystemLibrary("dxgi", .{});
    mod.linkSystemLibrary("openvr_api", .{});
    // openxr_loader.lib only has undecorated names
    // zig dlltool -m i386 -k -d thirdparty/openxr/openxr_loader.def -l thirdparty/lib/libopenxr_loader.a
    mod.addObjectFile(b.path("thirdparty/lib/libopenxr_loader.a"));
    mod.linkSystemLibrary("user32", .{});
    mod.linkSystemLibrary("version", .{});

    b.installFile("assets/valve_hands/LICENSE", "Plugins/openRBRVR/Valve-hand-models-LICENSE.txt");
    b.installFile("LICENSE", "Plugins/openRBRVR/LICENSE");
    b.installFile("README.md", "Plugins/openRBRVR/README.openrbrvr.txt");
    b.installFile("README_FR.md", "Plugins/openRBRVR/README_FR.openrbrvr.txt");
    b.installFile("README_CZ.md", "Plugins/openRBRVR/README_CZ.openrbrvr.txt");
    b.installFile("thirdparty/dll/openvr_api.dll", "openvr_api.dll");
    b.installFile("thirdparty/dll/openxr_loader.dll", "openxr_loader.dll");
    b.installDirectory(.{ .source_dir = b.path("thirdparty/quad-views-foveated"), .install_dir = .prefix, .install_subdir = "Plugins/openRBRVR/quad-views-foveated" });
    b.installDirectory(.{ .source_dir = b.path("thirdparty/obsmirror"), .install_dir = .prefix, .install_subdir = "Plugins/openRBRVR/obsmirror" });

    b.getInstallStep().dependOn(&b.addInstallArtifact(dll, .{
        .dest_dir = .{ .override = .{ .custom = "Plugins" } },
        .implib_dir = .disabled,
        .pdb_dir = .{ .override = .{ .custom = "Plugins" } },
    }).step);

    b.getInstallStep().dependOn(&b.addInstallArtifact(d3d9, .{
        .dest_dir = .{ .override = .{ .custom = "." } },
        .h_dir = .disabled,
        .implib_dir = .disabled,
    }).step);

    // For compile_commands.json
    var targets: std.ArrayListUnmanaged(*std.Build.Step.Compile) = .empty;
    targets.append(b.allocator, dll) catch @panic("OOM");
    var cdb_step = zcc.createStep(b, "cdb", targets.toOwnedSlice(b.allocator) catch @panic("OOM"));
    cdb_step.dependOn(&d3d9.step);
}
