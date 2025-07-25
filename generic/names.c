#if DEBUG
#include <tcl.h>
#include "names.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int adjectivesc = 0;
static const char* adjectives[] = {
	"Different", "Used", "Important", "Every", "Large", "Available", "Popular",
	"Able", "Basic", "Known", "Various", "Difficult", "Several", "United",
	"Historical", "Hot", "Useful", "Mental", "Scared", "Additional",
	"Emotional", "Old", "Political", "Similar", "Healthy", "Financial",
	"Medical", "Traditional", "Federal", "Entire", "Strong", "Actual",
	"Significant", "Successful", "Electrical", "Expensive", "Pregnant",
	"Intelligent", "Interesting", "Poor", "Happy", "Responsible", "Cute",
	"Helpful", "Recent", "Willing", "Nice", "Wonderful", "Impossible",
	"Serious", "Huge", "Rare", "Technical", "Typical", "Competitive",
	"Critical", "Electronic", "Immediate", "Aware", "Educational",
	"Environmental", "Global", "Legal", "Relevant", "Accurate", "Capable",
	"Dangerous", "Dramatic", "Efficient", "Powerful", "Foreign", "Hungry",
	"Practical", "Psychological", "Severe", "Suitable", "Numerous",
	"Sufficient", "Unusual", "Consistent", "Cultural", "Existing", "Famous",
	"Pure", "Afraid", "Obvious", "Careful", "Latter", "Unhappy", "Acceptable",
	"Aggressive", "Boring", "Distinct", "Eastern", "Logical", "Reasonable",
	"Strict", "Administrative", "Automatic", "Civil", "Former", "Massive",
	"Southern", "Unfair", "Visible", "Alive", "Angry", "Desperate", "Exciting",
	"Friendly", "Lucky", "Realistic", "Sorry", "Ugly", "Unlikely", "Anxious",
	"Comprehensive", "Curious", "Impressive", "Informal", "Inner", "Pleasant",
	"Sexual", "Sudden", "Terrible", "Unable", "Weak", "Wooden", "Asleep",
	"Confident", "Conscious", "Decent", "Embarrassed", "Guilty", "Lonely",
	"Mad", "Nervous", "Odd", "Remarkable", "Substantial", "Suspicious", "Tall",
	"Tiny", "More", "Some", "One", "All", "Many", "Most", "Other", "Such",
	"Even", "New", "Just", "Good", "Any", "Each", "Much", "Own", "Great",
	"Another", "Same", "Few", "Free", "Right", "Still", "Best", "Public",
	"Human", "Both", "Local", "Sure", "Better", "General", "Specific",
	"Enough", "Long", "Small", "Less", "High", "Certain", "Little", "Common",
	"Next", "Simple", "Hard", "Past", "Big", "Possible", "Particular", "Real",
	"Major", "Personal", "Current", "Left", "National", "Least", "Natural",
	"Physical", "Short", "Last", "Single", "Individual", "Main", "Potential",
	"Professional", "International", "Lower", "Open", "According",
	"Alternative", "Special", "Working", "True", "Whole", "Clear", "Dry",
	"Easy", "Cold", "Commercial", "Full", "Low", "Primary", "Worth",
	"Necessary", "Positive", "Present", "Close", "Creative", "Green", "Late",
	"Fit", "Glad", "Proper", "Complex", "Content", "Due", "Effective",
	"Middle", "Regular", "Fast", "Independent", "Original", "Wide",
	"Beautiful", "Complete", "Active", "Negative", "Safe", "Visual", "Wrong",
	"Ago", "Quick", "Ready", "Straight", "White", "Direct", "Excellent",
	"Extra", "Junior", "Pretty", "Unique", "Classic", "Final", "Overall",
	"Private", "Separate", "Western", "Alone", "Familiar", "Official",
	"Perfect", "Bright", "Broad", "Comfortable", "Flat", "Rich", "Warm",
	"Young", "Heavy", "Valuable", "Correct", "Leading", "Slow", "Clean",
	"Fresh", "Normal", "Secret", "Tough", "Brown", "Cheap", "Deep",
	"Objective", "Secure", "Thin", "Chemical", "Cool", "Extreme", "Exact",
	"Fair", "Fine", "Formal", "Opposite", "Remote", "Total", "Vast", "Lost",
	"Smooth", "Dark", "Double", "Equal", "Firm", "Frequent", "Internal",
	"Sensitive", "Constant", "Minor", "Previous", "Raw", "Soft", "Solid",
	"Weird", "Amazing", "Annual", "Busy", "Dead", "False", "Round", "Sharp",
	"Thick", "Wise", "Equivalent", "Initial", "Narrow", "Nearby", "Proud",
	"Spiritual", "Wild", "Adult", "Apart", "Brief", "Crazy", "Prior", "Rough",
	"Sad", "Sick", "Strange", "External", "Illegal", "Loud", "Mobile", "Nasty",
	"Ordinary", "Royal", "Senior", "Super", "Tight", "Upper", "Yellow",
	"Dependent", "Funny", "Gross", "Ill", "Spare", "Sweet", "Upstairs",
	"Usual", "Brave", "Calm", "Dirty", "Downtown", "Grand", "Honest", "Loose",
	"Male", "Quiet", "Brilliant", "Dear", "Drunk", "Empty", "Female",
	"Inevitable", "Neat", "Ok", "Representative", "Silly", "Slight", "Smart",
	"Stupid", "Temporary", "Weekly", "That", "This", "What", "Which", "Time",
	"These", "Work", "No", "Only", "Then", "First", "Money", "Over",
	"Business", "His", "Game", "Think", "After", "Life", "Day", "Home",
	"Economy", "Away", "Either", "Fat", "Key", "Training", "Top", "Level",
	"Far", "Fun", "House", "Kind", "Future", "Action", "Live", "Period",
	"Subject", "Mean", "Stock", "Chance", "Beginning", "Upset", "Chicken",
	"Head", "Material", "Salt", "Car", "Appropriate", "Inside", "Outside",
	"Standard", "Medium", "Choice", "North", "Square", "Born", "Capital",
	"Shot", "Front", "Living", "Plastic", "Express", "Feeling", "Otherwise",
	"Plus", "Savings", "Animal", "Budget", "Minute", "Character", "Maximum",
	"Novel", "Plenty", "Select", "Background", "Forward", "Glass", "Joint",
	"Master", "Red", "Vegetable", "Ideal", "Kitchen", "Mother", "Party",
	"Relative", "Signal", "Street", "Connect", "Minimum", "Sea", "South",
	"Status", "Daughter", "Hour", "Trick", "Afternoon", "Gold", "Mission",
	"Agent", "Corner", "East", "Neither", "Parking", "Routine", "Swimming",
	"Winter", "Airline", "Designer", "Dress", "Emergency", "Evening",
	"Extension", "Holiday", "Horror", "Mountain", "Patient", "Proof", "West",
	"Wine", "Expert", "Native", "Opening", "Silver", "Waste", "Plane",
	"Leather", "Purple", "Specialist", "Bitter", "Incident", "Motor",
	"Pretend", "Prize", "Resident",
	NULL
};

int nounsc = 0;
static const char* nouns[] = {
	"People", "History", "Way", "Art", "World", "Information", "Map", "Two",
	"Family", "Government", "Health", "System", "Computer", "Meat", "Year",
	"Thanks", "Music", "Person", "Reading", "Method", "Data", "Food",
	"Understanding", "Theory", "Law", "Bird", "Literature", "Problem",
	"Software", "Control", "Knowledge", "Power", "Ability", "Economics",
	"Love", "Internet", "Television", "Science", "Library", "Nature", "Fact",
	"Product", "Idea", "Temperature", "Investment", "Area", "Society",
	"Activity", "Story", "Industry", "Media", "Thing", "Oven", "Community",
	"Definition", "Safety", "Quality", "Development", "Language", "Management",
	"Player", "Variety", "Video", "Week", "Security", "Country", "Exam",
	"Movie", "Organization", "Equipment", "Physics", "Analysis", "Policy",
	"Series", "Thought", "Basis", "Boyfriend", "Direction", "Strategy",
	"Technology", "Army", "Camera", "Freedom", "Paper", "Environment", "Child",
	"Instance", "Month", "Truth", "Marketing", "University", "Writing",
	"Article", "Department", "Difference", "Goal", "News", "Audience",
	"Fishing", "Growth", "Income", "Marriage", "User", "Combination",
	"Failure", "Meaning", "Medicine", "Philosophy", "Teacher", "Communication",
	"Night", "Chemistry", "Disease", "Disk", "Energy", "Nation", "Road",
	"Role", "Soup", "Advertising", "Location", "Success", "Addition",
	"Apartment", "Education", "Math", "Moment", "Painting", "Politics",
	"Attention", "Decision", "Event", "Property", "Shopping", "Student",
	"Wood", "Competition", "Distribution", "Entertainment", "Office",
	"Population", "President", "Unit", "Category", "Cigarette", "Context",
	"Introduction", "Opportunity", "Performance", "Driver", "Flight", "Length",
	"Magazine", "Newspaper", "Relationship", "Teaching", "Cell", "Dealer",
	"Finding", "Lake", "Member", "Message", "Phone", "Scene", "Appearance",
	"Association", "Concept", "Customer", "Death", "Discussion", "Housing",
	"Inflation", "Insurance", "Mood", "Woman", "Advice", "Blood", "Effort",
	"Expression", "Importance", "Opinion", "Payment", "Reality",
	"Responsibility", "Situation", "Skill", "Statement", "Wealth",
	"Application", "City", "County", "Depth", "Estate", "Foundation",
	"Grandmother", "Heart", "Perspective", "Photo", "Recipe", "Studio",
	"Topic", "Collection", "Depression", "Imagination", "Passion",
	"Percentage", "Resource", "Setting", "Ad", "Agency", "College",
	"Connection", "Criticism", "Debt", "Description", "Memory", "Patience",
	"Secretary", "Solution", "Administration", "Aspect", "Attitude",
	"Director", "Personality", "Psychology", "Recommendation", "Response",
	"Selection", "Storage", "Version", "Alcohol", "Argument", "Complaint",
	"Contract", "Emphasis", "Highway", "Loss", "Membership", "Possession",
	"Preparation", "Steak", "Union", "Agreement", "Cancer", "Currency",
	"Employment", "Engineering", "Entry", "Interaction", "Mixture",
	"Preference", "Region", "Republic", "Tradition", "Virus", "Actor",
	"Classroom", "Delivery", "Device", "Difficulty", "Drama", "Election",
	"Engine", "Football", "Guidance", "Hotel", "Owner", "Priority",
	"Protection", "Suggestion", "Tension", "Variation", "Anxiety",
	"Atmosphere", "Awareness", "Bath", "Bread", "Candidate", "Climate",
	"Comparison", "Confusion", "Construction", "Elevator", "Emotion",
	"Employee", "Employer", "Guest", "Height", "Leadership", "Mall", "Manager",
	"Operation", "Recording", "Sample", "Transportation", "Charity", "Cousin",
	"Disaster", "Editor", "Efficiency", "Excitement", "Extent", "Feedback",
	"Guitar", "Homework", "Leader", "Mom", "Outcome", "Permission",
	"Presentation", "Promotion", "Reflection", "Refrigerator", "Resolution",
	"Revenue", "Session", "Singer", "Tennis", "Basket", "Bonus", "Cabinet",
	"Childhood", "Church", "Clothes", "Coffee", "Dinner", "Drawing", "Hair",
	"Hearing", "Initiative", "Judgment", "Lab", "Measurement", "Mode", "Mud",
	"Orange", "Poetry", "Police", "Possibility", "Procedure", "Queen", "Ratio",
	"Relation", "Restaurant", "Satisfaction", "Sector", "Signature",
	"Significance", "Song", "Tooth", "Town", "Vehicle", "Volume", "Wife",
	"Accident", "Airport", "Appointment", "Arrival", "Assumption", "Baseball",
	"Chapter", "Committee", "Conversation", "Database", "Enthusiasm", "Error",
	"Explanation", "Farmer", "Gate", "Girl", "Hall", "Historian", "Hospital",
	"Injury", "Instruction", "Maintenance", "Manufacturer", "Meal",
	"Perception", "Pie", "Poem", "Presence", "Proposal", "Reception",
	"Replacement", "Revolution", "River", "Son", "Speech", "Tea", "Village",
	"Warning", "Winner", "Worker", "Writer", "Assistance", "Breath", "Buyer",
	"Chest", "Chocolate", "Conclusion", "Contribution", "Cookie", "Courage",
	"Dad", "Desk", "Drawer", "Establishment", "Examination", "Garbage",
	"Grocery", "Honey", "Impression", "Improvement", "Independence", "Insect",
	"Inspection", "Inspector", "King", "Ladder", "Menu", "Penalty", "Piano",
	"Potato", "Profession", "Professor", "Quantity", "Reaction", "Requirement",
	"Salad", "Sister", "Supermarket", "Tongue", "Weakness", "Wedding",
	"Affair", "Ambition", "Analyst", "Apple", "Assignment", "Assistant",
	"Bathroom", "Bedroom", "Beer", "Birthday", "Celebration", "Championship",
	"Cheek", "Client", "Consequence", "Departure", "Diamond", "Dirt", "Ear",
	"Fortune", "Friendship", "Funeral", "Gene", "Girlfriend", "Hat",
	"Indication", "Intention", "Lady", "Midnight", "Negotiation", "Obligation",
	"Passenger", "Pizza", "Platform", "Poet", "Pollution", "Recognition",
	"Reputation", "Shirt", "Sir", "Speaker", "Stranger", "Surgery", "Sympathy",
	"Tale", "Throat", "Trainer", "Uncle", "Youth", "Time", "Work", "Film",
	"Water", "Money", "Example", "While", "Business", "Study", "Game", "Life",
	"Form", "Air", "Day", "Place", "Number", "Part", "Field", "Fish", "Back",
	"Process", "Heat", "Hand", "Experience", "Job", "Book", "End", "Point",
	"Type", "Home", "Economy", "Value", "Body", "Market", "Guide", "Interest",
	"State", "Radio", "Course", "Company", "Price", "Size", "Card", "List",
	"Mind", "Trade", "Line", "Care", "Group", "Risk", "Word", "Fat", "Force",
	"Key", "Light", "Training", "Name", "School", "Top", "Amount", "Level",
	"Order", "Practice", "Research", "Sense", "Service", "Piece", "Web",
	"Boss", "Sport", "Fun", "House", "Page", "Term", "Test", "Answer", "Sound",
	"Focus", "Matter", "Kind", "Soil", "Board", "Oil", "Picture", "Access",
	"Garden", "Range", "Rate", "Reason", "Future", "Site", "Demand",
	"Exercise", "Image", "Case", "Cause", "Coast", "Action", "Age", "Bad",
	"Boat", "Record", "Result", "Section", "Building", "Mouse", "Cash",
	"Class", "Nothing", "Period", "Plan", "Store", "Tax", "Side", "Subject",
	"Space", "Rule", "Stock", "Weather", "Chance", "Figure", "Man", "Model",
	"Source", "Beginning", "Earth", "Program", "Chicken", "Design", "Feature",
	"Head", "Material", "Purpose", "Question", "Rock", "Salt", "Act", "Birth",
	"Car", "Dog", "Object", "Scale", "Sun", "Note", "Profit", "Rent", "Speed",
	"Style", "War", "Bank", "Craft", "Half", "Inside", "Outside", "Standard",
	"Bus", "Exchange", "Eye", "Fire", "Position", "Pressure", "Stress",
	"Advantage", "Benefit", "Box", "Frame", "Issue", "Step", "Cycle", "Face",
	"Item", "Metal", "Paint", "Review", "Room", "Screen", "Structure", "View",
	"Account", "Ball", "Discipline", "Medium", "Share", "Balance", "Bit",
	"Black", "Bottom", "Choice", "Gift", "Impact", "Machine", "Shape", "Tool",
	"Wind", "Address", "Average", "Career", "Culture", "Morning", "Pot",
	"Sign", "Table", "Task", "Condition", "Contact", "Credit", "Egg", "Hope",
	"Ice", "Network", "North", "Square", "Attempt", "Date", "Effect", "Link",
	"Post", "Star", "Voice", "Capital", "Challenge", "Friend", "Self", "Shot",
	"Brush", "Couple", "Debate", "Exit", "Front", "Function", "Lack", "Living",
	"Plant", "Plastic", "Spot", "Summer", "Taste", "Theme", "Track", "Wing",
	"Brain", "Button", "Click", "Desire", "Foot", "Gas", "Influence", "Notice",
	"Rain", "Wall", "Base", "Damage", "Distance", "Feeling", "Pair", "Savings",
	"Staff", "Sugar", "Target", "Text", "Animal", "Author", "Budget",
	"Discount", "File", "Ground", "Lesson", "Minute", "Officer", "Phase",
	"Reference", "Register", "Sky", "Stage", "Stick", "Title", "Trouble",
	"Bowl", "Bridge", "Campaign", "Character", "Club", "Edge", "Evidence",
	"Fan", "Letter", "Lock", "Maximum", "Novel", "Option", "Pack", "Park",
	"Plenty", "Quarter", "Skin", "Sort", "Weight", "Baby", "Background",
	"Carry", "Dish", "Factor", "Fruit", "Glass", "Joint", "Master", "Muscle",
	"Red", "Strength", "Traffic", "Trip", "Vegetable", "Appeal", "Chart",
	"Gear", "Ideal", "Kitchen", "Land", "Log", "Mother", "Net", "Party",
	"Principle", "Relative", "Sale", "Season", "Signal", "Spirit", "Street",
	"Tree", "Wave", "Belt", "Bench", "Commission", "Copy", "Drop", "Minimum",
	"Path", "Progress", "Project", "Sea", "South", "Status", "Stuff", "Ticket",
	"Tour", "Angle", "Blue", "Breakfast", "Confidence", "Daughter", "Degree",
	"Doctor", "Dot", "Dream", "Duty", "Essay", "Father", "Fee", "Finance",
	"Hour", "Juice", "Limit", "Luck", "Milk", "Mouth", "Peace", "Pipe", "Seat",
	"Stable", "Storm", "Substance", "Team", "Trick", "Afternoon", "Bat",
	"Beach", "Blank", "Catch", "Chain", "Consideration", "Cream", "Crew",
	"Detail", "Gold", "Interview", "Kid", "Mark", "Match", "Mission", "Pain",
	"Pleasure", "Score", "Screw", "Sex", "Shop", "Shower", "Suit", "Tone",
	"Window", "Agent", "Band", "Block", "Bone", "Calendar", "Cap", "Coat",
	"Contest", "Corner", "Court", "Cup", "District", "Door", "East", "Finger",
	"Garage", "Guarantee", "Hole", "Hook", "Implement", "Layer", "Lecture",
	"Lie", "Manner", "Meeting", "Nose", "Parking", "Partner", "Profile",
	"Respect", "Rice", "Routine", "Schedule", "Swimming", "Telephone", "Tip",
	"Winter", "Airline", "Bag", "Battle", "Bed", "Bill", "Bother", "Cake",
	"Code", "Curve", "Designer", "Dimension", "Dress", "Ease", "Emergency",
	"Evening", "Extension", "Farm", "Fight", "Gap", "Grade", "Holiday",
	"Horror", "Horse", "Host", "Husband", "Loan", "Mistake", "Mountain",
	"Nail", "Noise", "Occasion", "Package", "Patient", "Pause", "Phrase",
	"Proof", "Race", "Relief", "Sand", "Sentence", "Shoulder", "Smoke",
	"Stomach", "String", "Tourist", "Towel", "Vacation", "West", "Wheel",
	"Wine", "Arm", "Aside", "Associate", "Bet", "Blow", "Border", "Branch",
	"Breast", "Brother", "Buddy", "Bunch", "Chip", "Coach", "Cross",
	"Document", "Draft", "Dust", "Expert", "Floor", "God", "Golf", "Habit",
	"Iron", "Judge", "Knife", "Landscape", "League", "Mail", "Mess", "Native",
	"Opening", "Parent", "Pattern", "Pin", "Pool", "Pound", "Request",
	"Salary", "Shame", "Shelter", "Shoe", "Silver", "Tackle", "Tank", "Trust",
	"Assist", "Bake", "Bar", "Bell", "Bike", "Blame", "Boy", "Brick", "Chair",
	"Closet", "Clue", "Collar", "Comment", "Conference", "Devil", "Diet",
	"Fear", "Fuel", "Glove", "Jacket", "Lunch", "Monitor", "Mortgage", "Nurse",
	"Pace", "Panic", "Peak", "Plane", "Reward", "Row", "Sandwich", "Shock",
	"Spite", "Spray", "Surprise", "Till", "Transition", "Weekend", "Welcome",
	"Yard", "Alarm", "Bend", "Bicycle", "Bite", "Blind", "Bottle", "Cable",
	"Candle", "Clerk", "Cloud", "Concert", "Counter", "Flower", "Grandfather",
	"Harm", "Knee", "Lawyer", "Leather", "Load", "Mirror", "Neck", "Pension",
	"Plate", "Purple", "Ruin", "Ship", "Skirt", "Slice", "Snow", "Specialist",
	"Stroke", "Switch", "Trash", "Tune", "Zone", "Anger", "Award", "Bid",
	"Bitter", "Boot", "Bug", "Camp", "Candy", "Carpet", "Cat", "Champion",
	"Channel", "Clock", "Comfort", "Cow", "Crack", "Engineer", "Entrance",
	"Fault", "Grass", "Guy", "Hell", "Highlight", "Incident", "Island", "Joke",
	"Jury", "Leg", "Lip", "Mate", "Motor", "Nerve", "Passage", "Pen", "Pride",
	"Priest", "Prize", "Promise", "Resident", "Resort", "Ring", "Roof", "Rope",
	"Sail", "Scheme", "Script", "Sock", "Station", "Toe", "Tower", "Truck",
	"Witness", "A", "You", "It", "Can", "Will", "If", "One", "Many", "Most",
	"Other", "Use", "Make", "Good", "Look", "Help", "Go", "Great", "Being",
	"Few", "Might", "Still", "Public", "Read", "Keep", "Start", "Give",
	"Human", "Local", "General", "She", "Specific", "Long", "Play", "Feel",
	"High", "Tonight", "Put", "Common", "Set", "Change", "Simple", "Past",
	"Big", "Possible", "Particular", "Today", "Major", "Personal", "Current",
	"National", "Cut", "Natural", "Physical", "Show", "Try", "Check", "Second",
	"Call", "Move", "Pay", "Let", "Increase", "Single", "Individual", "Turn",
	"Ask", "Buy", "Guard", "Hold", "Main", "Offer", "Potential",
	"Professional", "International", "Travel", "Cook", "Alternative",
	"Following", "Special", "Working", "Whole", "Dance", "Excuse", "Cold",
	"Commercial", "Low", "Purchase", "Deal", "Primary", "Worth", "Fall",
	"Necessary", "Positive", "Produce", "Search", "Present", "Spend", "Talk",
	"Creative", "Tell", "Cost", "Drive", "Green", "Support", "Glad", "Remove",
	"Return", "Run", "Complex", "Due", "Effective", "Middle", "Regular",
	"Reserve", "Independent", "Leave", "Original", "Reach", "Rest", "Serve",
	"Watch", "Beautiful", "Charge", "Active", "Break", "Negative", "Safe",
	"Stay", "Visit", "Visual", "Affect", "Cover", "Report", "Rise", "Walk",
	"White", "Beyond", "Junior", "Pick", "Unique", "Anything", "Classic",
	"Final", "Lift", "Mix", "Private", "Stop", "Teach", "Western", "Concern",
	"Familiar", "Fly", "Official", "Broad", "Comfortable", "Gain", "Maybe",
	"Rich", "Save", "Stand", "Young", "Fail", "Heavy", "Hello", "Lead",
	"Listen", "Valuable", "Worry", "Handle", "Leading", "Meet", "Release",
	"Sell", "Finish", "Normal", "Press", "Ride", "Secret", "Spread", "Spring",
	"Tough", "Wait", "Brown", "Deep", "Display", "Flow", "Hit", "Objective",
	"Shoot", "Touch", "Cancel", "Chemical", "Cry", "Dump", "Extreme", "Push",
	"Conflict", "Eat", "Fill", "Formal", "Jump", "Kick", "Opposite", "Pass",
	"Pitch", "Remote", "Total", "Treat", "Vast", "Abuse", "Beat", "Burn",
	"Deposit", "Print", "Raise", "Sleep", "Somewhere", "Advance", "Anywhere",
	"Consist", "Dark", "Double", "Draw", "Equal", "Fix", "Hire", "Internal",
	"Join", "Kill", "Sensitive", "Tap", "Win", "Attack", "Claim", "Constant",
	"Drag", "Drink", "Guess", "Minor", "Pull", "Raw", "Soft", "Solid", "Wear",
	"Weird", "Wonder", "Annual", "Count", "Dead", "Doubt", "Feed", "Forever",
	"Impress", "Nobody", "Repeat", "Round", "Sing", "Slide", "Strip",
	"Whereas", "Wish", "Combine", "Command", "Dig", "Divide", "Equivalent",
	"Hang", "Hunt", "Initial", "March", "Mention", "Smell", "Spiritual",
	"Survey", "Tie", "Adult", "Brief", "Crazy", "Escape", "Gather", "Hate",
	"Prior", "Repair", "Rough", "Sad", "Scratch", "Sick", "Strike", "Employ",
	"External", "Hurt", "Illegal", "Laugh", "Lay", "Mobile", "Nasty",
	"Ordinary", "Respond", "Royal", "Senior", "Split", "Strain", "Struggle",
	"Swim", "Train", "Upper", "Wash", "Yellow", "Convert", "Crash",
	"Dependent", "Fold", "Funny", "Grab", "Hide", "Miss", "Permit", "Quote",
	"Recover", "Resolve", "Roll", "Sink", "Slip", "Spare", "Suspect", "Sweet",
	"Swing", "Twist", "Upstairs", "Usual", "Abroad", "Brave", "Calm",
	"Concentrate", "Estimate", "Grand", "Male", "Mine", "Prompt", "Quiet",
	"Refuse", "Regret", "Reveal", "Rush", "Shake", "Shift", "Shine", "Steal",
	"Suck", "Surround", "Anybody", "Bear", "Brilliant", "Dare", "Dear",
	"Delay", "Drunk", "Female", "Hurry", "Inevitable", "Invite", "Kiss",
	"Neat", "Pop", "Punch", "Quit", "Reply", "Representative", "Resist", "Rip",
	"Rub", "Silly", "Smile", "Spell", "Stretch", "Stupid", "Tear", "Temporary",
	"Tomorrow", "Wake", "Wrap", "Yesterday",
	NULL
};


TCL_DECLARE_MUTEX(things_mutex);
static int				hash_tables_initialized = 0;
static int				maxlen_adjective = 0;
static int				maxlen_noun = 0;
static Tcl_HashTable	things;			// Map void* -> name
static Tcl_HashTable	names;			// Map name -> void*

Tcl_ThreadDataKey		outbuf;

static void init()
{
	int				new;
	Tcl_HashEntry*	he = NULL;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	Tcl_MutexLock(&things_mutex);
	if (hash_tables_initialized == 0) {
		srandom((int)ts.tv_nsec);

		for (adjectivesc=0; adjectives[adjectivesc]; adjectivesc++) {
			int len = strlen(adjectives[adjectivesc]);
			if (len > maxlen_adjective) maxlen_adjective = len;
		}

		for (nounsc=0; nouns[nounsc]; nounsc++) {
			int len = strlen(nouns[nounsc]);
			if (len > maxlen_noun) maxlen_noun = len;
		}

		Tcl_InitHashTable(&things, TCL_ONE_WORD_KEYS);
		Tcl_InitHashTable(&names, TCL_STRING_KEYS);

		he = Tcl_CreateHashEntry(&names, "NULL", &new);
		Tcl_SetHashValue(he, NULL);

		hash_tables_initialized = 1;
	}
	Tcl_MutexUnlock(&things_mutex);
}


void names_shutdown()
{
	Tcl_MutexLock(&things_mutex);
	if (hash_tables_initialized) {
		Tcl_HashEntry*	he;
		Tcl_HashSearch	search;

		for (he = Tcl_FirstHashEntry(&things, &search); he; he = Tcl_NextHashEntry(&search)) {
			char*	chosen = Tcl_GetHashValue(he);
			ckfree(chosen);
		}
		Tcl_DeleteHashTable(&things);
		Tcl_DeleteHashTable(&names);
		hash_tables_initialized = 0;
	}
	Tcl_MutexUnlock(&things_mutex);
	Tcl_MutexFinalize(&things_mutex);
}


const char* randwords()
{
	const int	maxlen = 41;
	int			adj = random() % adjectivesc;
	int			noun = random() % nounsc;
	char*		out = Tcl_GetThreadData(&outbuf, maxlen);

	snprintf(out, maxlen, "%s%s", adjectives[adj], nouns[noun]);

	return out;
}


void* thing(const char *const name)
{
	Tcl_HashEntry*	he_name = NULL;

	if (hash_tables_initialized == 0) init();

	Tcl_MutexLock(&things_mutex);
	he_name = Tcl_FindHashEntry(&things, name);
	Tcl_MutexUnlock(&things_mutex);

	return Tcl_GetHashValue(he_name);
}


const char* name(const void *const thing)
{
	int				new;
	Tcl_HashEntry*	he_thing = NULL;
	Tcl_HashEntry*	he_name = NULL;
	const char*		candidate = NULL;
	char*			chosen = NULL;
	int				chosenlen;

	if (thing == NULL)
		return "NULL";

	if (hash_tables_initialized == 0) init();

	Tcl_MutexLock(&things_mutex);
	he_thing = Tcl_CreateHashEntry(&things, thing, &new);
	Tcl_MutexUnlock(&things_mutex);
	if (!new)
		return Tcl_GetHashValue(he_thing);

	new = 0;
	while (!new) {
		candidate = randwords();
		Tcl_MutexLock(&things_mutex);
		he_name = Tcl_CreateHashEntry(&names, candidate, &new);
		Tcl_MutexUnlock(&things_mutex);
	}

	chosenlen = strlen(candidate)+1;
	chosen = ckalloc(chosenlen);
	memcpy(chosen, candidate, chosenlen);
	Tcl_SetHashValue(he_name, thing);
	Tcl_SetHashValue(he_thing, chosen);

	return chosen;
}

#endif
// vim: ts=4 shiftwidth=4
