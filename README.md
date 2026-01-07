# Roseverse Auth Generator

This application generates authentication data for Roseverse on the Nintendo 3DS.

> [!IMPORTANT]
> You will need a Pretendo Network ID (PNID) linked and active to use this application. Authentication data for Roseverse cannot be generated for Nintendo Network IDs (NNID). Make sure you have selected "Pretendo" in nimbus.

# Usage

- Download the [latest release 3DSX](https://github.com/ZeroSkill1/RoseverseAuthGenerator/releases/latest) and place it into the `3ds` folder on your SD card.
- Launch Roseverse Auth Generator in the Homebrew Launcher.
- The app will obtain information about your PNID and use it to generate authentication data. This data will be placed under `sd:/olive`.

> [!WARNING]
> Please do not modify or remove any files in `sd:/olive` after running the app.

# Building from source

All you need is the latest version of [devkitARM](https://devkitpro.org/wiki/Getting_Started).

Clone the repository and run `make`.

# License

See [LICENSE](./LICENSE).