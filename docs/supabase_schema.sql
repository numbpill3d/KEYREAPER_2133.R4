-- PN-2133 Supabase Database Schema
-- designed for field durability, not academic purity
-- tested under stress conditions

-- table for standard tag logging operations
CREATE TABLE tag_logs (
    id BIGSERIAL PRIMARY KEY,
    uid VARCHAR(32) NOT NULL,
    timestamp BIGINT NOT NULL,
    tag_type VARCHAR(50) NOT NULL,
    device_id VARCHAR(20) NOT NULL DEFAULT 'PN2133',
    data TEXT,
    location_info TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    
    -- indexes for fast queries under load
    INDEX idx_tag_logs_uid (uid),
    INDEX idx_tag_logs_timestamp (timestamp),
    INDEX idx_tag_logs_device (device_id),
    INDEX idx_tag_logs_created (created_at)
);

-- table for entropy anomalies and paranormal events
-- when things get weird, we log them separately
CREATE TABLE paranormal_events (
    id BIGSERIAL PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    entropy_value INTEGER NOT NULL,
    deviation INTEGER NOT NULL,
    baseline INTEGER NOT NULL,
    device_id VARCHAR(20) NOT NULL DEFAULT 'PN2133',
    event_type VARCHAR(50) NOT NULL DEFAULT 'ENTROPY_SPIKE',
    concurrent_tag_uid VARCHAR(32),
    environmental_data TEXT,
    notes TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    
    -- indexes for anomaly analysis
    INDEX idx_paranormal_timestamp (timestamp),
    INDEX idx_paranormal_device (device_id),
    INDEX idx_paranormal_event_type (event_type),
    INDEX idx_paranormal_deviation (deviation),
    INDEX idx_paranormal_created (created_at)
);

-- enable row level security (RLS) for basic protection
ALTER TABLE tag_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE paranormal_events ENABLE ROW LEVEL SECURITY;

-- basic RLS policies for anon key access
-- adjust these based on your security requirements
CREATE POLICY "Allow anon insert on tag_logs" ON tag_logs
    FOR INSERT TO anon
    WITH CHECK (true);

CREATE POLICY "Allow anon select on tag_logs" ON tag_logs
    FOR SELECT TO anon
    USING (true);

CREATE POLICY "Allow anon insert on paranormal_events" ON paranormal_events
    FOR INSERT TO anon
    WITH CHECK (true);

CREATE POLICY "Allow anon select on paranormal_events" ON paranormal_events
    FOR SELECT TO anon
    USING (true);

-- view for combined analysis of tags and anomalies
-- useful for correlation analysis
CREATE VIEW tag_anomaly_correlation AS
SELECT 
    tl.uid,
    tl.timestamp as tag_timestamp,
    tl.tag_type,
    pe.timestamp as anomaly_timestamp,
    pe.deviation,
    pe.event_type,
    ABS(tl.timestamp - pe.timestamp) as time_diff_ms
FROM tag_logs tl
CROSS JOIN paranormal_events pe
WHERE ABS(tl.timestamp - pe.timestamp) <= 5000  -- 5 second correlation window
AND tl.device_id = pe.device_id
ORDER BY time_diff_ms ASC;

-- function to clean old logs (run monthly to prevent storage bloat)
CREATE OR REPLACE FUNCTION cleanup_old_logs(days_to_keep INTEGER DEFAULT 90)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    -- clean tag logs older than specified days
    DELETE FROM tag_logs 
    WHERE created_at < NOW() - INTERVAL '1 day' * days_to_keep;
    
    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    
    -- clean paranormal events older than specified days
    DELETE FROM paranormal_events 
    WHERE created_at < NOW() - INTERVAL '1 day' * days_to_keep;
    
    GET DIAGNOSTICS deleted_count = deleted_count + ROW_COUNT;
    
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- example usage: SELECT cleanup_old_logs(30); -- keep 30 days

-- trigger function to auto-correlate nearby anomalies with tags
CREATE OR REPLACE FUNCTION check_tag_anomaly_correlation()
RETURNS TRIGGER AS $$
BEGIN
    -- when a new tag is logged, check for recent paranormal events
    IF TG_TABLE_NAME = 'tag_logs' THEN
        UPDATE paranormal_events 
        SET concurrent_tag_uid = NEW.uid,
            notes = COALESCE(notes, '') || ' [AUTO] Tag ' || NEW.uid || ' detected within correlation window'
        WHERE ABS(timestamp - NEW.timestamp) <= 2000  -- 2 second window
        AND device_id = NEW.device_id
        AND concurrent_tag_uid IS NULL;
    END IF;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- create trigger for automatic correlation
CREATE TRIGGER tag_anomaly_auto_correlate
    AFTER INSERT ON tag_logs
    FOR EACH ROW
    EXECUTE FUNCTION check_tag_anomaly_correlation();

-- useful queries for field analysis:

-- get recent tag activity
-- SELECT uid, tag_type, to_timestamp(timestamp/1000) as scan_time 
-- FROM tag_logs 
-- WHERE created_at > NOW() - INTERVAL '1 hour'
-- ORDER BY timestamp DESC;

-- get anomaly patterns
-- SELECT event_type, COUNT(*), AVG(deviation), MAX(deviation)
-- FROM paranormal_events 
-- WHERE created_at > NOW() - INTERVAL '24 hours'
-- GROUP BY event_type;

-- get correlated events (tags + anomalies within 5 seconds)
-- SELECT * FROM tag_anomaly_correlation 
-- WHERE time_diff_ms <= 5000
-- ORDER BY tag_timestamp DESC;