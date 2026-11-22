# SPDX-FileCopyrightText: 2026 nat <nat@nekopon.pl>
#
# SPDX-License-Identifier: CC0-1.0

{
  description = "rtremotesf - remote GUI for rtorrent";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      inherit (nixpkgs) lib;

      systems = lib.systems.flakeExposed;
      forAllSystems = lib.genAttrs systems;
      pkgsFor = system: import nixpkgs { inherit system; };

      packageFor = system: (pkgsFor system).callPackage ./nix/package.nix { };
    in
    {
      packages = forAllSystems (
        system:
        let
          package = packageFor system;
        in
        {
          default = package;
          rtremotesf = package;
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ (packageFor system) ];
            packages = with pkgs; [
              clang-tools
              cmake
              gettext
              ninja
              pkg-config
            ];
          };
        }
      );

      formatter = forAllSystems (system: (pkgsFor system).nixfmt);
    };
}
