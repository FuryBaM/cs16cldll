# R14 Command Menu Unicode

Test package for Steam CS 1.6 GoldSrc `client.dll`.

## Changes

- command menu labels are rendered through a Unicode-safe path instead of passing UTF-8 directly to legacy VGUI1 labels;
- UTF-8 BOM and Cyrillic labels are supported;
- command menu nodes with more than eight items use pages;
- keys `1`-`8` select items, `9` opens the next page, and `0` goes back or closes the menu;
- returning from nested nodes preserves the parent page;
- normal and diagnostic Win32 x86 DLLs are included in the test archive distributed separately.

## Package

`cs16-steam-goldsrc-r14-commandmenu-unicode.zip`

SHA-256:

```
3e22fc8649ed4873485b383a3789b6f85caf9bfa3da93f49c0f0dbeba2b52005
```

The archive contains:

- `cstrike/cl_dlls/client.dll`;
- `diagnostic/client-r14-commandmenu-unicode-trace.dll`;
- fallback R13 DLL;
- installation notes and checksums.

The source implementation must be committed separately after reconstructing the lost temporary checkout and reproducing the R14 build.