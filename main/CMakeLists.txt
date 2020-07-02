idf_component_register(SRCS "smartdemon_main.c"
					SRCS "mqtt.c"
					SRCS "http_get.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES server_root_cert.pem)