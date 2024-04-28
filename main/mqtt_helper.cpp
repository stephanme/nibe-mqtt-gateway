// extra file for testing
// https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901241
// assumes a valid topic filter, e.g. wildcards only for complete topics
bool mqtt_match_topic(const char* topic, const char* filter) {
    while (*topic && *filter) {
        if (*filter == '+') {
            // skip until next '/'
            while (*topic && *topic != '/') {
                topic++;
            }
            filter++;
        } else if (*filter == '#') {
            return true;
        } else if (*topic != *filter) {
            return false;
        } else {
            topic++;
            filter++;
        }
    }
    if (*topic == 0) {
        // single-level wildcard matches exactly one level
        if (*filter == '+') {
            filter++;
        }
        // The multi-level wildcard represents the parent (and must be the last filter char).
        if (*filter == '/' && *(filter + 1) == '#') {
            filter += 2;
        }
    }
    return *topic == *filter;
}