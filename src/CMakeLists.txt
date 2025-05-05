file(GLOB_RECURSE src_f CONFIGURE_DEPENDS
    "*.cpp"
)

target_sources(texgui PRIVATE ${src_f})
