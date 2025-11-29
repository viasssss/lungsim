#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include "diagnostics.h"
#include "indices.h"
#include "geometry.h"
#include "ventilation.h"

/*
 * Standalone C executable replicating the behaviour of
 * lung-group-examples/ventilation_Swan2011/ventilation_Swan2011.py
 *
 * Usage:
 *   ventilation_swan2011 "[4,5,6,7,8,9,10,13,15,17]" <derivation> [seed]
 *
 * Performs:
 *   set_diagnostics_on(False)
 *   ventilation_indices()
 *   define_node_geometry("../geometry/P2BRP268-H12816_Airway_Full.ipnode")
 *   define_1d_elements("../geometry/P2BRP268-H12816_Airway_Full.ipelem")
 *   define_rad_from_geom("horsf", h_ratio=1.16, start_from="inlet", start_rad=8.74,
 *                        group_type="all", group_options="", modify_indices=airway_index,
 *                        deviation=derivation, seed=seed)
 *   append_units()
 *   evaluate_vent()
 *
 * Prints the resulting double returned by evaluate_vent to stdout.
 */

static int *parse_int_list(const char *json, int *out_len) {
    /* Minimal parser for a JSON-ish array of integers: [1,2,3] */
    const char *p = json;
    while (isspace((unsigned char)*p)) p++;
    if (*p != '[') {
        fprintf(stderr, "Error: airway_index must start with '['\n");
        return NULL;
    }
    p++; /* skip '[' */
    int capacity = 16;
    int count = 0;
    int *arr = (int *)malloc(sizeof(int) * capacity);
    if (!arr) {
        fprintf(stderr, "Error: memory allocation failed\n");
        return NULL;
    }
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (*p == ']') { /* end */
            p++;
            break;
        }
        int sign = 1;
        if (*p == '-') { sign = -1; p++; }
        if (!isdigit((unsigned char)*p)) {
            fprintf(stderr, "Error: expected digit in airway_index list\n");
            free(arr);
            return NULL;
        }
        long val = 0;
        while (isdigit((unsigned char)*p)) {
            val = val * 10 + (*p - '0');
            p++;
        }
        val *= sign;
        if (count == capacity) {
            capacity *= 2;
            int *tmp = (int *)realloc(arr, sizeof(int) * capacity);
            if (!tmp) {
                fprintf(stderr, "Error: memory allocation failed\n");
                free(arr);
                return NULL;
            }
            arr = tmp;
        }
        arr[count++] = (int)val;
        while (isspace((unsigned char)*p)) p++;
        if (*p == ',') {
            p++; /* continue */
            continue;
        } else if (*p == ']') {
            p++;
            break;
        } else {
            /* Unexpected character */
            if (*p) {
                fprintf(stderr, "Error: unexpected character '%c' in airway_index list\n", *p);
                free(arr);
                return NULL;
            }
        }
    }
    *out_len = count;
    return arr;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <airway_index_json> <derivation> [seed]\n", argv[0]);
        return 1;
    }

    const char *airway_json = argv[1];
    double derivation = atof(argv[2]);
    int seed = 0;
    if (argc > 3) {
        seed = atoi(argv[3]);
    }

    int airway_len = 0;
    int *airway_indices = parse_int_list(airway_json, &airway_len);
    if (!airway_indices) {
        return 1;
    }

    /* Disable diagnostics (matches set_diagnostics_on(False)) */
    set_diagnostics_on(0);

    /* Setup indices */
    ventilation_indices();

    /* Resolve geometry file paths. Try multiple candidate locations for portability. */
    const char *env_geom_dir = getenv("AETHER_GEOMETRY_DIR");
    const char *fname_node = "P2BRP268-H12816_Airway_Full.ipnode";
    const char *fname_elem = "P2BRP268-H12816_Airway_Full.ipelem";
    char path_node[1024];
    char path_elem[1024];

    const char *candidates[] = {
        env_geom_dir, /* environment override */
        "../geometry",
        "../lung-group-examples/geometry",
        "../../lung-group-examples/geometry",
        "/Users/august/Documents/lung/lung-group-examples/geometry" /* absolute (current repo layout) */
    };

    int found = 0;
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        if (!candidates[i] || strlen(candidates[i]) == 0) continue;
        snprintf(path_node, sizeof(path_node), "%s/%s", candidates[i], fname_node);
        snprintf(path_elem, sizeof(path_elem), "%s/%s", candidates[i], fname_elem);
        FILE *f = fopen(path_node, "r");
        if (f) {
            fclose(f);
            found = 1;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "Error: Could not locate geometry files. Set AETHER_GEOMETRY_DIR.\n");
        free(airway_indices);
        return 1;
    }

    define_node_geometry(path_node);
    define_1d_elements(path_elem);

    /* Parameters from Python script */
    const char *order_system = "horsf"; /* ordering system */
    double h_ratio = 1.16;              /* Horsfield ratio */
    const char *start_from = "inlet";   /* starting location */
    double trachea_rad = 8.74;          /* trachea radius */
    const char *group_type = "all";     /* apply to all */
    const char *group_options = "";     /* no options */

    /* Define radii based on geometry */
    define_rad_from_geom(order_system, h_ratio, start_from, trachea_rad,
                         group_type, group_options, airway_indices, airway_len,
                         derivation, seed);

    /* Append terminal units */
    append_units();

    /* Change into example directory (to access Parameters/ files) similar to Python script behaviour */
    char cwd[PATH_MAX];
    int cwd_ok = 0;
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        cwd_ok = 1;
    } else {
        fprintf(stderr, "Warning: getcwd failed; continuing.\n");
    }
    const char *env_example_dir = getenv("AETHER_EXAMPLE_DIR");
    const char *example_candidates[] = {
        env_example_dir,
        "../lung-group-examples/ventilation_Swan2011",
        "../../lung-group-examples/ventilation_Swan2011",
        "/Users/august/Documents/lung/lung-group-examples/ventilation_Swan2011"
    };
    int changed_dir = 0;
    for (size_t i = 0; i < sizeof(example_candidates)/sizeof(example_candidates[0]); ++i) {
        if (!example_candidates[i] || strlen(example_candidates[i]) == 0) continue;
        if (chdir(example_candidates[i]) == 0) {
            changed_dir = 1;
            break;
        }
    }
    if (!changed_dir) {
        fprintf(stderr, "Warning: Could not change directory to example folder; Parameters files may not be found.\n");
    }

    /* Evaluate ventilation model */
    double result = evaluate_vent();

    /* Restore working directory */
    if (changed_dir && cwd_ok) {
        if (chdir(cwd) != 0) {
            fprintf(stderr, "Warning: Failed to restore working directory to %s\n", cwd);
        }
    }

    /* Print result with high precision to match Python output */
    // fprintf(stderr, "Debug: evaluate_vent returned: %.17g\n", result);
    printf("%.17g\n", result);

    free(airway_indices);
    return 0;
}
