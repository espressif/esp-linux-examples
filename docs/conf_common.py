# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

from esp_docs.conf_docs import *  # noqa: F401,F403

languages = ["en"]

project_slug = "esp-linux-examples"

# The repository is hosted on GitLab, so do not generate GitHub source links.
html_context["display_github"] = False
html_context["display_vcs_links"] = False
