/*
* This file is part of EternalPatcherLinux (https://github.com/PowerBall253/EternalPatcherLinux).
* Copyright (C) 2021 PowerBall253
*
* EternalPatcherLinux is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* EternalPatcherLinux is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with EternalPatcherLinux. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eternalpatcher.h"

#define OFFSET_PATCH 0
#define PATTERN_PATCH 1

const int patcher_version = 3;
GArray *gamebuilds;

// Check if there is any patch defs update available
bool update_available(void)
{
    if (get_update_server() == -1)
        return false;

    char *patch_defs_md5 = get_md5_hash("EternalPatcher.def");

    if (patch_defs_md5[0] == '\0')
        return true;

    char patch_defs_md5_webpage[256];
    sprintf(patch_defs_md5_webpage, "http://%s/EternalPatcher_v%d.md5", update_server, patcher_version);

    char *latest_patch_defs_md5 = get_latest_patch_defs_md5(patch_defs_md5_webpage);

    if (latest_patch_defs_md5 == NULL || *latest_patch_defs_md5 == '\0')
        return false;

    bool update_available = false;

    if (strcmp(patch_defs_md5, latest_patch_defs_md5) != 0)
        update_available = true;

    free(patch_defs_md5);
    free(latest_patch_defs_md5);

    return update_available;
}

// Load patch defs from file
int load_patch_defs(void)
{
    FILE *patch_defs = fopen("EternalPatcher.def", "rb");

    if (!patch_defs) {
        fprintf(stderr, "ERROR: Failed to open patches file!\n");
        return -1;
    }

    char current_line[256];
    gamebuilds = g_array_new(false, false, sizeof(struct GameBuild));

    while (fgets(current_line, 256, patch_defs) != NULL) {
        rm_whitespace(current_line);

        if (strchr(current_line, '#') == current_line)
            continue;

        char **data_def;
        int data_def_len = 0;

        split_string(current_line, '=', &data_def, &data_def_len);

        if (data_def_len <= 1)
            continue;

        if (strcmp(data_def[0], "patch") != 0) {
            char **gamebuild_data;
            int gamebuild_data_len = 0;

            split_string(data_def[1], ':', &gamebuild_data, &gamebuild_data_len);

            if (gamebuild_data_len != 3)
                continue;

            for (int i = 0; i < gamebuild_data_len; i++)
                rm_whitespace(gamebuild_data[i]);

            char **patch_group_ids;
            int patch_group_ids_len = 0;

            split_string(gamebuild_data[2], ',', &patch_group_ids, &patch_group_ids_len);

            struct GameBuild new_gamebuild;
            new_gamebuild.id = strdup(data_def[0]);
            new_gamebuild.exe_filename = strdup(gamebuild_data[0]);
            new_gamebuild.md5_checksum = strdup(gamebuild_data[1]);
            new_gamebuild.patch_group_ids = malloc(patch_group_ids_len * sizeof(char*));
            new_gamebuild.patch_group_ids_len = patch_group_ids_len;
            new_gamebuild.offset_patches = g_array_new(false, false, sizeof(struct OffsetPatch));
            new_gamebuild.pattern_patches = g_array_new(false, false, sizeof(struct PatternPatch));

            if (!new_gamebuild.id || !new_gamebuild.exe_filename || !new_gamebuild.md5_checksum || !new_gamebuild.patch_group_ids) {
                fprintf(stderr, "ERROR: Failed to allocate memory for gamebuild!\n");
                exit(1);
            }

            for (int i = 0; i < patch_group_ids_len; i++)
                new_gamebuild.patch_group_ids[i] = strdup(patch_group_ids[i]);

            g_array_append_val(gamebuilds, new_gamebuild);

            free(gamebuild_data);
            free(patch_group_ids);
        }
        else {
            char **patch_data;
            int patch_data_len = 0;

            split_string(data_def[1], ':', &patch_data, &patch_data_len);

            if (patch_data_len != 5)
                continue;

            for (int i = 0; i < patch_data_len; i++)
                rm_whitespace(patch_data[i]);

            if (strlen(patch_data[4]) % 2 != 0)
                continue;

            int patch_type;

            if (strcmp(patch_data[1], "offset") == 0) {
                patch_type = OFFSET_PATCH;
            }
            else if (strcmp(patch_data[1], "pattern") == 0 && strlen(patch_data[3]) % 2 == 0) {
                patch_type = PATTERN_PATCH;
            }
            else {
                continue;
            }

            char **patch_group_ids;
            int patch_group_ids_len = 0;

            split_string(patch_data[2], ',', &patch_group_ids, &patch_group_ids_len);

            if (patch_group_ids_len == 0)
                continue;
            
            for (int i = 0; i < patch_group_ids_len; i++)
                rm_whitespace(patch_group_ids[i]);

            unsigned char *hex_patch = hex_to_bytes(patch_data[4]);

            if (patch_type == OFFSET_PATCH) {
                struct OffsetPatch offset_patch;

                offset_patch.description = strdup(patch_data[0]);
                offset_patch.offset = strtol(patch_data[3], NULL, 16);
                offset_patch.patch_byte_array = malloc(strlen(patch_data[4]) / 2);
                offset_patch.patch_byte_array_len = (int)strlen(patch_data[4]) / 2;

                if (!offset_patch.description || !offset_patch.patch_byte_array) {
                    fprintf(stderr, "ERROR: Failed to allocate memory for offset patch!\n");
                    exit(1);
                }

                memcpy(offset_patch.patch_byte_array, hex_patch, offset_patch.patch_byte_array_len);

                free(hex_patch);

                if (offset_patch.description[0] == '\0')
                    continue;

                for (int i = 0; i < patch_group_ids_len; i++) {
                    for (int j = 0; j < gamebuilds->len; j++) {
                        struct GameBuild gamebuild_j = g_array_index(gamebuilds, struct GameBuild, j);

                        bool found = false;

                        for (int k = 0; k < gamebuild_j.patch_group_ids_len; k++) {
                            if (strcmp(gamebuild_j.patch_group_ids[k], patch_group_ids[i]) == 0)
                                found = true;
                        }

                        if (!found)
                            continue;

                        bool already_exists = false;

                        for (int k = 0; k < gamebuild_j.offset_patches->len; k++) {
                            struct OffsetPatch current_patch = g_array_index(gamebuild_j.offset_patches, struct OffsetPatch, k);

                            if (strcmp(current_patch.description, offset_patch.description) == 0) {
                                already_exists = true;
                                break;
                            }
                        }

                        if (already_exists)
                            break;

                        g_array_append_val(gamebuild_j.offset_patches, offset_patch);
                    }
                }
            }
            else {
                unsigned char *hex_pattern = hex_to_bytes(patch_data[3]);

                struct PatternPatch pattern_patch;

                pattern_patch.description = strdup(patch_data[0]);
                pattern_patch.pattern = malloc(strlen(patch_data[3]) / 2);
                pattern_patch.pattern_len = (int)strlen(patch_data[3]) / 2;
                pattern_patch.patch_byte_array = malloc(strlen(patch_data[4]) / 2);
                pattern_patch.patch_byte_array_len = (int)strlen(patch_data[4]) / 2;

                if (!pattern_patch.description || !pattern_patch.pattern || !pattern_patch.patch_byte_array) {
                    fprintf(stderr, "ERROR: Failed to allocate memory for pattern patch!\n");
                    exit(1);
                }

                memcpy(pattern_patch.pattern, hex_pattern, pattern_patch.pattern_len);
                memcpy(pattern_patch.patch_byte_array, hex_patch, pattern_patch.patch_byte_array_len);

                free(hex_pattern);
                free(hex_patch);

                if (pattern_patch.description[0] == '\0')
                    continue;
                
                for (int i = 0; i < patch_group_ids_len; i++) {
                    for (int j = 0; j < gamebuilds->len; j++) {
                        struct GameBuild gamebuild_j = g_array_index(gamebuilds, struct GameBuild, j);

                        bool found = false;

                        for (int k = 0; k < gamebuild_j.patch_group_ids_len; k++) {
                            if (strcmp(gamebuild_j.patch_group_ids[k], patch_group_ids[i]) == 0)
                                found = true;
                        }

                        if (!found)
                            continue;

                        bool already_exists = false;

                        for (int k = 0; k < gamebuild_j.pattern_patches->len; k++) {
                            struct PatternPatch current_patch = g_array_index(gamebuild_j.pattern_patches, struct PatternPatch, k);

                            if (strcmp(current_patch.description, pattern_patch.description) == 0) {
                                already_exists = true;
                                break;
                            }
                        }

                        if (already_exists)
                            break;

                        g_array_append_val(gamebuild_j.pattern_patches, pattern_patch);
                    }
                }
            }

            free(patch_data);
            free(patch_group_ids);
        }

        free(data_def);
    }

    return 0;
}

// Check if there are any patches loaded
bool any_patches_loaded(void)
{   
    for (int i = 0; i < gamebuilds->len; i++) {
        struct GameBuild gamebuild_i = g_array_index(gamebuilds, struct GameBuild, i);

        if (gamebuild_i.offset_patches->len > 0 || gamebuild_i.pattern_patches->len > 0)
            return true;
    }

    return false;
}

// Get the given executable's gamebuild from the patch defs
struct GameBuild *get_gamebuild(const char *filepath)
{
    if (*filepath == '\0')
        return NULL;

    char *file_md5 = get_md5_hash(filepath);

    for (int i = 0; i < gamebuilds->len; i++) {
        struct GameBuild* gamebuild_i = &g_array_index(gamebuilds, struct GameBuild, i);

        if (strcmp(gamebuild_i->md5_checksum, file_md5) == 0) {
            free(file_md5);
            return gamebuild_i;
        }
    }

    free(file_md5);
    return NULL;
}

// Apply the loaded patches to the executable
struct PatchingResult *apply_patches(const char *binary_filepath, GArray *offset_patches, GArray *pattern_patches)
{
    struct PatchingResult *patching_results = malloc((pattern_patches->len + offset_patches->len) * sizeof(struct PatchingResult));

    if (!patching_results) {
        fprintf(stderr, "ERROR: Failed to allocate memory for patching results!\n");
        exit(1);
    }

    for (int i = 0; i < offset_patches->len; i++) {
        struct OffsetPatch *offset_patch_i = &g_array_index(offset_patches, struct OffsetPatch, i);

        struct PatchingResult patching_result = {
            strdup(offset_patch_i->description),
            offset_apply(binary_filepath, offset_patch_i)
        };

        patching_results[pattern_patches->len + i] = patching_result;

        free(offset_patch_i->description);
        free(offset_patch_i->patch_byte_array);
    }

    for (int i = 0; i < pattern_patches->len; i++) {
        struct PatternPatch *pattern_patch_i = &g_array_index(pattern_patches, struct PatternPatch, i);

        struct PatchingResult patching_result = {
            strdup(pattern_patch_i->description),
            pattern_apply(binary_filepath, pattern_patch_i)
        };

        patching_results[i] = patching_result;

        free(pattern_patch_i->description);
        free(pattern_patch_i->pattern);
        free(pattern_patch_i->patch_byte_array);
    }

    return patching_results;
}
