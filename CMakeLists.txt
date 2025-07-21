idf_component_register(SRCS "mjpeg.c" "riff.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES driver
                    REQUIRES log sd types
                    )

