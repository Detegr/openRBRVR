{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    zig = { url = github:mitchellh/zig-overlay; };
    zls = { url = github:zigtools/zls/0.15.1; };
  };

  outputs = { self, nixpkgs, zig, zls }: {
    devShell.x86_64-linux = let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in pkgs.mkShell {
      buildInputs = [
        pkgs.clang-tools
        zig.packages.x86_64-linux."0.15.2"
        zls.packages.x86_64-linux.zls
      ];
    };
  };
}
