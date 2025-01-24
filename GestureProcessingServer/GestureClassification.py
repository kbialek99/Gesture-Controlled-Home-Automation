import math

def is_thumbs_up(hand_landmarks, handedness):
    """
    Detects if the hand gesture is a "thumbs up" based on Mediapipe landmarks.

    Parameters:
        hand_landmarks: Mediapipe Hand landmarks for one hand (21 points).
        handedness: The detected hand (e.g., "Right" or "Left").

    Returns:
        True if the gesture is "thumbs up," otherwise False.
    """
    # Extract landmarks for easier access
    landmarks = hand_landmarks.landmark

    # Thumb key points
    thumb_tip = landmarks[4]
    thumb_ip = landmarks[3]
    thumb_mcp = landmarks[2]
    thumb_cmc = landmarks[1]

    # Index finger key points
    index_mcp = landmarks[5]
    index_pip = landmarks[6]
    index_dip = landmarks[7]
    index_tip = landmarks[8]

    # Step 1: Check if thumb is extended upward
    if handedness == "Right":
        # For the right hand, the thumb tip should be above (smaller y-coord) the thumb MCP joint
        is_thumb_extended = thumb_tip.y < thumb_mcp.y
    else:  # Left hand
        # For the left hand, the thumb tip should also be above the thumb MCP joint
        is_thumb_extended = thumb_tip.y < thumb_mcp.y

    # Step 2: Ensure other fingers are folded
    # Check if all index finger joints (except MCP) are below the thumb MCP joint
    are_fingers_folded = (
            index_tip.y > thumb_mcp.y and
            index_dip.y > thumb_mcp.y and
            index_pip.y > thumb_mcp.y
    )

    # Step 3: Thumb alignment (thumb pointing upwards)
    # Calculate the angle between the thumb's tip, IP, and MCP joints
    def calculate_angle(a, b, c):
        # Angle between points a (thumb_tip), b (thumb_ip), and c (thumb_mcp)
        ba = [a.x - b.x, a.y - b.y, a.z - b.z]
        bc = [c.x - b.x, c.y - b.y, c.z - b.z]
        dot_product = sum(ba[i] * bc[i] for i in range(3))
        mag_ba = math.sqrt(sum(ba[i] ** 2 for i in range(3)))
        mag_bc = math.sqrt(sum(bc[i] ** 2 for i in range(3)))
        return math.degrees(math.acos(dot_product / (mag_ba * mag_bc)))

    thumb_angle = calculate_angle(thumb_tip, thumb_ip, thumb_mcp)

    # A reasonable thumb angle for a "thumbs up" is between 150 and 180 degrees
    is_thumb_pointing_up = 150 <= thumb_angle <= 180

    # Step 4: Combine conditions
    return is_thumb_extended and are_fingers_folded and is_thumb_pointing_up
