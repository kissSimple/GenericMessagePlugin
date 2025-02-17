// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "MessageTagContainer.h"
#include "Engine/DataTable.h"
#include "Templates/UniquePtr.h"
#include "UnrealCompatibility.h"
#include "MessageTagsManager.generated.h"

class UMessageTagsList;
struct FStreamableHandle;
class FNativeMessageTag;

USTRUCT(BlueprintInternalUseOnly)
struct FMessageParameter
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MessageTag)
	FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MessageTag)
	FName Type;
};

/** Simple struct for a table row in the message tag table and element in the ini list */
USTRUCT()
struct MESSAGETAGS_API FMessageTagTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	/** Tag specified in the table */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MessageTag)
	FName Tag;

	/** Developer comment clarifying the usage of a particular tag, not user facing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MessageTag)
	FString DevComment;

	/** Constructors */
	FMessageTagTableRow() {}

	UPROPERTY(EditAnywhere, Category = MessageTag)
	TArray<FMessageParameter> Parameters;

	UPROPERTY(EditAnywhere, Category = MessageTag)
	TArray<FMessageParameter> ResponseTypes;

	FMessageTagTableRow(FName InTag, const FString& InDevComment = TEXT(""), const TArray<FMessageParameter>& InParameters = {}, const TArray<FMessageParameter>& InResTypes = {})
		: Tag(InTag)
		, DevComment(InDevComment)
		, Parameters(InParameters)
		, ResponseTypes(InResTypes)
	{
	}

	FMessageTagTableRow(FMessageTagTableRow const& Other);

	/** Assignment/Equality operators */
	FMessageTagTableRow& operator=(FMessageTagTableRow const& Other);
	bool operator==(FMessageTagTableRow const& Other) const;
	bool operator!=(FMessageTagTableRow const& Other) const;
	bool operator<(FMessageTagTableRow const& Other) const;
};

/** Simple struct for a table row in the restricted message tag table and element in the ini list */
USTRUCT()
struct MESSAGETAGS_API FRestrictedMessageTagTableRow : public FMessageTagTableRow
{
	GENERATED_USTRUCT_BODY()

	/** Tag specified in the table */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MessageTag)
	bool bAllowNonRestrictedChildren;

	/** Constructors */
	FRestrictedMessageTagTableRow() : bAllowNonRestrictedChildren(false) {}
	FRestrictedMessageTagTableRow(FName InTag, const FString& InDevComment = TEXT(""), bool InAllowNonRestrictedChildren = false, const TArray<FMessageParameter>& InParameters = {}, const TArray<FMessageParameter>& InResTypes = {})
		: FMessageTagTableRow(InTag, InDevComment, InParameters, InResTypes)
		, bAllowNonRestrictedChildren(InAllowNonRestrictedChildren)
	{
	}
	FRestrictedMessageTagTableRow(FRestrictedMessageTagTableRow const& Other);

	/** Assignment/Equality operators */
	FRestrictedMessageTagTableRow& operator=(FRestrictedMessageTagTableRow const& Other);
	bool operator==(FRestrictedMessageTagTableRow const& Other) const;
	bool operator!=(FRestrictedMessageTagTableRow const& Other) const;
};

UENUM()
enum class EMessageTagSourceType : uint8
{
	Native,				// Was added from C++ code
	DefaultTagList,		// The default tag list in DefaultMessageTags.ini
	TagList,			// Another tag list from an ini in tags/*.ini
	RestrictedTagList,	// Restricted tags from an ini
	DataTable,			// From a DataTable
	Invalid,			// Not a real source
};

UENUM()
enum class EMessageTagSelectionType : uint8
{
	None,
	NonRestrictedOnly,
	RestrictedOnly,
	All
};

/** Struct defining where message tags are loaded/saved from. Mostly for the editor */
USTRUCT()
struct MESSAGETAGS_API FMessageTagSource
{
	GENERATED_USTRUCT_BODY()

	/** Name of this source */
	UPROPERTY()
	FName SourceName;

	/** Type of this source */
	UPROPERTY()
	EMessageTagSourceType SourceType;

	/** If this is bound to an ini object for saving, this is the one */
	UPROPERTY()
	class UMessageTagsList* SourceTagList;

	/** If this has restricted tags and is bound to an ini object for saving, this is the one */
	UPROPERTY()
	class URestrictedMessageTagsList* SourceRestrictedTagList;

	FMessageTagSource() 
		: SourceName(NAME_None), SourceType(EMessageTagSourceType::Invalid), SourceTagList(nullptr), SourceRestrictedTagList(nullptr)
	{
	}

	FMessageTagSource(FName InSourceName, EMessageTagSourceType InSourceType, UMessageTagsList* InSourceTagList = nullptr, URestrictedMessageTagsList* InSourceRestrictedTagList = nullptr) 
		: SourceName(InSourceName), SourceType(InSourceType), SourceTagList(InSourceTagList), SourceRestrictedTagList(InSourceRestrictedTagList)
	{
	}

	/** Returns the config file that created this source, if valid */
	FString GetConfigFileName() const;

	static FName GetNativeName();

	static FName GetDefaultName();

#if WITH_EDITOR
	static FName GetFavoriteName();

	static void SetFavoriteName(FName TagSourceToFavorite);

	static FName GetTransientEditorName();
#endif
};

/** Struct describing the places to look for ini search paths */
struct FMessageTagSearchPathInfo
{
	/** Which sources should be loaded from this path */
	TArray<FName> SourcesInPath;

	/** Config files to load from, will normally correspond to FoundSources */
	TArray<FString> TagIniList;

	/** True if this path has already been searched */
	bool bWasSearched = false;

	/** True if the tags in sources have been added to the current tree */
	bool bWasAddedToTree = false;

	FORCEINLINE void Reset()
	{
		SourcesInPath.Reset();
		TagIniList.Reset();
		bWasSearched = false;
		bWasAddedToTree = false;
	}

	FORCEINLINE bool IsValid()
	{
		return bWasSearched && bWasAddedToTree;
	}
};

/** Simple tree node for message tags, this stores metadata about specific tags */
USTRUCT()
struct FMessageTagNode
{
	GENERATED_USTRUCT_BODY()
	FMessageTagNode(){};

	/** Simple constructor, passing redundant data for performance */
	FMessageTagNode(FName InTag, FName InFullTag, TSharedPtr<FMessageTagNode> InParentNode, bool InIsExplicitTag, bool InIsRestrictedTag, bool InAllowNonRestrictedChildren);

	/** Returns a correctly constructed container with only this tag, useful for doing container queries */
	FORCEINLINE const FMessageTagContainer& GetSingleTagContainer() const { return CompleteTagWithParents; }

	/**
	 * Get the complete tag for the node, including all parent tags, delimited by periods
	 * 
	 * @return Complete tag for the node
	 */
	FORCEINLINE const FMessageTag& GetCompleteTag() const { return CompleteTagWithParents.Num() > 0 ? CompleteTagWithParents.MessageTags[0] : FMessageTag::EmptyTag; }
	FORCEINLINE FName GetCompleteTagName() const { return GetCompleteTag().GetTagName(); }
	FORCEINLINE FString GetCompleteTagString() const { return GetCompleteTag().ToString(); }

	/**
	 * Get the simple tag for the node (doesn't include any parent tags)
	 * 
	 * @return Simple tag for the node
	 */
	FORCEINLINE FName GetSimpleTagName() const { return Tag; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE TArray< TSharedPtr<FMessageTagNode> >& GetChildTagNodes() { return ChildTags; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE const TArray< TSharedPtr<FMessageTagNode> >& GetChildTagNodes() const { return ChildTags; }

	/**
	 * Get the parent tag node of this node
	 * 
	 * @return The parent tag node of this node
	 */
	FORCEINLINE TSharedPtr<FMessageTagNode> GetParentTagNode() const { return ParentNode; }

	/**
	* Get the net index of this node
	*
	* @return The net index of this node
	*/
	FORCEINLINE FMessageTagNetIndex GetNetIndex() const {  check(NetIndex != INVALID_TAGNETINDEX); return NetIndex; }

	/** Reset the node of all of its values */
	MESSAGETAGS_API void ResetNode();

	/** Returns true if the tag was explicitly specified in code or data */
	FORCEINLINE bool IsExplicitTag() const {
#if WITH_EDITORONLY_DATA
		return bIsExplicitTag;
#endif
		return true;
	}

	/** Returns true if the tag is a restricted tag and allows non-restricted children */
	FORCEINLINE bool GetAllowNonRestrictedChildren() const { 
#if WITH_EDITORONLY_DATA
		return bAllowNonRestrictedChildren;  
#endif
		return true;
	}

	/** Returns true if the tag is a restricted tag */
	FORCEINLINE bool IsRestrictedMessageTag() const {
#if WITH_EDITORONLY_DATA
		return bIsRestrictedTag;
#endif
		return true;
	}

	TArray<FMessageParameter> Parameters;
	FORCEINLINE const FString& GetComment() const
	{
#if WITH_EDITORONLY_DATA
		return DevComment;
#else
		static FString EmptyString;
		return EmptyString;
#endif
	}

	TArray<FMessageParameter> ResponseTypes;

private:
	/** Raw name for this tag at current rank in the tree */
	FName Tag;

	/** This complete tag is at MessageTags[0], with parents in ParentTags[] */
	FMessageTagContainer CompleteTagWithParents;

	/** Child message tag nodes */
	TArray< TSharedPtr<FMessageTagNode> > ChildTags;

	/** Owner message tag node, if any */
	TSharedPtr<FMessageTagNode> ParentNode;
	
	/** Net Index of this node */
	FMessageTagNetIndex NetIndex;

#if WITH_EDITORONLY_DATA
	/** Package or config file this tag came from. This is the first one added. If None, this is an implicitly added tag */
	FName SourceName;

	/** Comment for this tag */
	FString DevComment;

	/** If this is true then the tag can only have normal tag children if bAllowNonRestrictedChildren is true */
	uint8 bIsRestrictedTag : 1;

	/** If this is true then any children of this tag must come from the restricted tags */
	uint8 bAllowNonRestrictedChildren : 1;

	/** If this is true then the tag was explicitly added and not only implied by its child tags */
	uint8 bIsExplicitTag : 1;

	/** If this is true then at least one tag that inherits from this tag is coming from multiple sources. Used for updating UI in the editor. */
	uint8 bDescendantHasConflict : 1;

	/** If this is true then this tag is coming from multiple sources. No descendants can be changed on this tag until this is resolved. */
	uint8 bNodeHasConflict : 1;

	/** If this is true then at least one tag that this tag descends from is coming from multiple sources. This tag and it's descendants can't be changed in the editor. */
	uint8 bAncestorHasConflict : 1;
#endif 

	friend class UMessageTagsManager;
	friend class SMessageTagWidget;
};

/** Holds data about the tag dictionary, is in a singleton UObject */
UCLASS(config=Engine)
class MESSAGETAGS_API UMessageTagsManager : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Destructor */
	~UMessageTagsManager();

	/** Returns the global UMessageTagsManager manager */
	FORCEINLINE static UMessageTagsManager& Get()
	{
		if (SingletonManager == nullptr)
		{
			InitializeManager();
		}

		return *SingletonManager;
	}

	/** Returns possibly nullptr to the manager. Needed for some shutdown cases to avoid reallocating. */
	FORCEINLINE static UMessageTagsManager* GetIfAllocated() { return SingletonManager; }

	/**
	* Adds the message tags corresponding to the strings in the array TagStrings to OutTagsContainer
	*
	* @param TagStrings Array of strings to search for as tags to add to the tag container
	* @param OutTagsContainer Container to add the found tags to.
	* @param ErrorIfNotfound: ensure() that tags exists.
	*
	*/
	void RequestMessageTagContainer(const TArray<FString>& TagStrings, FMessageTagContainer& OutTagsContainer, bool bErrorIfNotFound=true) const;

	/**
	 * Gets the FMessageTag that corresponds to the TagName
	 *
	 * @param TagName The Name of the tag to search for
	 * @param ErrorIfNotfound: ensure() that tag exists.
	 * 
	 * @return Will return the corresponding FMessageTag or an empty one if not found.
	 */
	FMessageTag RequestMessageTag(FName TagName, bool ErrorIfNotFound=true) const;

	/** 
	 * Returns true if this is a valid message tag string (foo.bar.baz). If false, it will fill 
	 * @param TagString String to check for validity
	 * @param OutError If non-null and string invalid, will fill in with an error message
	 * @param OutFixedString If non-null and string invalid, will attempt to fix. Will be empty if no fix is possible
	 * @return True if this can be added to the tag dictionary, false if there's a syntax error
	 */
	bool IsValidMessageTagString(const FString& TagString, FText* OutError = nullptr, FString* OutFixedString = nullptr);

	/**
	 *	Searches for a message tag given a partial string. This is slow and intended mainly for console commands/utilities to make
	 *	developer life's easier. This will attempt to match as best as it can. If you pass "A.b" it will match on "A.b." before it matches "a.b.c".
	 */
	FMessageTag FindMessageTagFromPartialString_Slow(FString PartialString) const;

	/**
	 * Registers the given name as a message tag, and tracks that it is being directly referenced from code
	 * This can only be called during engine initialization, the table needs to be locked down before replication
	 *
	 * @param TagName The Name of the tag to add
	 * @param TagDevComment The developer comment clarifying the usage of the tag
	 * 
	 * @return Will return the corresponding FMessageTag
	 */
	FMessageTag AddNativeMessageTag(FName TagName, const FString& TagDevComment = TEXT("(Native)"));

private:
	void AddNativeMessageTag(FNativeMessageTag* TagSource);
	void RemoveNativeMessageTag(const FNativeMessageTag* TagSource);

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageTagSignatureChanged, FName);
	static FOnMessageTagSignatureChanged& OnMessageTagSignatureChanged();

	/** Call to flush the list of native tags, once called it is unsafe to add more */
	void DoneAddingNativeTags();

	static FSimpleMulticastDelegate& OnLastChanceToAddNativeTags();


	void CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleMulticastDelegate::FDelegate Delegate);

	/**
	 * Gets a Tag Container containing the supplied tag and all of it's parents as explicit tags
	 *
	 * @param MessageTag The Tag to use at the child most tag for this container
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FMessageTagContainer RequestMessageTagParents(const FMessageTag& MessageTag) const;

	/**
	 * Gets a Tag Container containing the all tags in the hierarchy that are children of this tag. Does not return the original tag
	 *
	 * @param MessageTag					The Tag to use at the parent tag
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FMessageTagContainer RequestMessageTagChildren(const FMessageTag& MessageTag) const;

	/** Returns direct parent MessageTag of this MessageTag, calling on x.y will return x */
	FMessageTag RequestMessageTagDirectParent(const FMessageTag& MessageTag) const;

	/**
	 * Helper function to get the stored TagContainer containing only this tag, which has searchable ParentTags
	 * @param MessageTag		Tag to get single container of
	 * @return					Pointer to container with this tag
	 */
	FORCEINLINE_DEBUGGABLE const FMessageTagContainer* GetSingleTagContainer(const FMessageTag& MessageTag) const
	{
		// Doing this with pointers to avoid a shared ptr reference count change
		const TSharedPtr<FMessageTagNode>* Node = MessageTagNodeMap.Find(MessageTag);

		if (Node)
		{
			return &(*Node)->GetSingleTagContainer();
		}
#if WITH_EDITOR
		// Check redirector
		if (GIsEditor && MessageTag.IsValid())
		{
			FMessageTag RedirectedTag = MessageTag;

			RedirectSingleMessageTag(RedirectedTag, nullptr);

			Node = MessageTagNodeMap.Find(RedirectedTag);

			if (Node)
			{
				return &(*Node)->GetSingleTagContainer();
			}
		}
#endif
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FMessageTagNode with the tag exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FMessageTagNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FMessageTagNode> FindTagNode(const FMessageTag& MessageTag) const
	{
		const TSharedPtr<FMessageTagNode>* Node = MessageTagNodeMap.Find(MessageTag);

		if (Node)
		{
			return *Node;
		}
#if WITH_EDITOR
		// Check redirector
		if (GIsEditor && MessageTag.IsValid())
		{
			FMessageTag RedirectedTag = MessageTag;

			RedirectSingleMessageTag(RedirectedTag, nullptr);

			Node = MessageTagNodeMap.Find(RedirectedTag);

			if (Node)
			{
				return *Node;
			}
		}
#endif
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FMessageTagNode with the name exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FMessageTagNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FMessageTagNode> FindTagNode(FName TagName) const
	{
		FMessageTag PossibleTag(TagName);
		return FindTagNode(PossibleTag);
	}

	/** Loads the tag tables referenced in the MessageTagSettings object */
	void LoadMessageTagTables(bool bAllowAsyncLoad = false);

	/** Loads tag inis contained in the specified path */
	void AddTagIniSearchPath(const FString& RootDir);

	/** Gets all the current directories to look for tag sources in */
	void GetTagSourceSearchPaths(TArray<FString>& OutPaths);

	/** Helper function to construct the message tag tree */
	void ConstructMessageTagTree();

	/** Helper function to destroy the message tag tree */
	void DestroyMessageTagTree();

	/** Splits a tag such as x.y.z into an array of names {x,y,z} */
	void SplitMessageTagFName(const FMessageTag& Tag, TArray<FName>& OutNames) const;

	/** Gets the list of all tags in the dictionary */
	void RequestAllMessageTags(FMessageTagContainer& TagContainer, bool OnlyIncludeDictionaryTags) const;

	/** Returns true if if the passed in name is in the tag dictionary and can be created */
	bool ValidateTagCreation(FName TagName) const;

	/** Returns the tag source for a given tag source name and type, or null if not found */
	const FMessageTagSource* FindTagSource(FName TagSourceName) const;

	/** Returns the tag source for a given tag source name and type, or null if not found */
	FMessageTagSource* FindTagSource(FName TagSourceName);

	/** Fills in an array with all tag sources of a specific type */
	void FindTagSourcesWithType(EMessageTagSourceType TagSourceType, TArray<const FMessageTagSource*>& OutArray) const;

	/**
	 * Check to see how closely two FMessageTags match. Higher values indicate more matching terms in the tags.
	 *
	 * @param MessageTagOne	The first tag to compare
	 * @param MessageTagTwo	The second tag to compare
	 *
	 * @return the length of the longest matching pair
	 */
	int32 MessageTagsMatchDepth(const FMessageTag& MessageTagOne, const FMessageTag& MessageTagTwo) const;

	/** Returns the number of parents a particular Message tag has.  Useful as a quick way to determine which tags may
	 * be more "specific" than other tags without comparing whether they are in the same hierarchy or anything else.
	 * Example: "TagA.SubTagA" has 2 Tag Nodes.  "TagA.SubTagA.LeafTagA" has 3 Tag Nodes.
	 */ 
	int32 GetNumberOfTagNodes(const FMessageTag& MessageTag) const;

	/** Returns true if we should import tags from UMessageTagsSettings objects (configured by INI files) */
	bool ShouldImportTagsFromINI() const;

	/** Should we print loading errors when trying to load invalid tags */
	bool ShouldWarnOnInvalidTags() const
	{
		return bShouldWarnOnInvalidTags;
	}

	/** Should we clear references to invalid tags loaded/saved in the editor */
	bool ShouldClearInvalidTags() const
	{
		return bShouldClearInvalidTags;
	}

	/** Should use fast replication */
	bool ShouldUseFastReplication() const
	{
		return bUseFastReplication;
	}

	/** Returns the hash of NetworkMessageTagNodeIndex */
	uint32 GetNetworkMessageTagNodeIndexHash() const { VerifyNetworkIndex(); return NetworkMessageTagNodeIndexHash; }

	/** Returns a list of the ini files that contain restricted tags */
	void GetRestrictedTagConfigFiles(TArray<FString>& RestrictedConfigFiles) const;

	/** Returns a list of the source files that contain restricted tags */
	void GetRestrictedTagSources(TArray<const FMessageTagSource*>& Sources) const;

	/** Returns a list of the owners for a restricted tag config file. May be empty */
	void GetOwnersForTagSource(const FString& SourceName, TArray<FString>& OutOwners) const;

	/** Notification that a tag container has been loaded via serialize */
	void MessageTagContainerLoaded(FMessageTagContainer& Container, FProperty* SerializingProperty) const;

	/** Notification that a message tag has been loaded via serialize */
	void SingleMessageTagLoaded(FMessageTag& Tag, FProperty* SerializingProperty) const;

	/** Handles redirectors for an entire container, will also error on invalid tags */
	void RedirectTagsForContainer(FMessageTagContainer& Container, FProperty* SerializingProperty) const;

	/** Handles redirectors for a single tag, will also error on invalid tag. This is only called for when individual tags are serialized on their own */
	void RedirectSingleMessageTag(FMessageTag& Tag, FProperty* SerializingProperty) const;

	/** Handles establishing a single tag from an imported tag name (accounts for redirects too). Called when tags are imported via text. */
	bool ImportSingleMessageTag(FMessageTag& Tag, FName ImportedTagName, bool bImportFromSerialize = false) const;

	/** Gets a tag name from net index and vice versa, used for replication efficiency */
	FName GetTagNameFromNetIndex(FMessageTagNetIndex Index) const;
	FMessageTagNetIndex GetNetIndexFromTag(const FMessageTag &InTag) const;

	/** Cached number of bits we need to replicate tags. That is, Log2(Number of Tags). Will always be <= 16. */
	int32 GetNetIndexTrueBitNum() const { VerifyNetworkIndex(); return NetIndexTrueBitNum; }

	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicatore "more" (more = second segment that is NetIndexTrueBitNum - NetIndexFirstBitSegment) */
	int32 GetNetIndexFirstBitSegment() const { VerifyNetworkIndex(); return NetIndexFirstBitSegment; }

	/** This is the actual value for an invalid tag "None". This is computed at runtime as (Total number of tags) + 1 */
	FMessageTagNetIndex GetInvalidTagNetIndex() const { VerifyNetworkIndex(); return InvalidTagNetIndex; }

	const TArray<TSharedPtr<FMessageTagNode>>& GetNetworkMessageTagNodeIndex() const { VerifyNetworkIndex(); return NetworkMessageTagNodeIndex; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageTagLoaded, const FMessageTag& /*Tag*/)
	FOnMessageTagLoaded OnMessageTagLoadedDelegate;

	/** Numbers of bits to use for replicating container size. This can be set via config. */
	int32 NumBitsForContainerSize;

private:
	/** Cached number of bits we need to replicate tags. That is, Log2(Number of Tags). Will always be <= 16. */
	int32 NetIndexTrueBitNum;

	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicatore "more" (more = second segment that is NetIndexTrueBitNum - NetIndexFirstBitSegment) */
	int32 NetIndexFirstBitSegment;

	/** This is the actual value for an invalid tag "None". This is computed at runtime as (Total number of tags) + 1 */
	FMessageTagNetIndex InvalidTagNetIndex;

public:

#if WITH_EDITOR
	/** Gets a Filtered copy of the MessageRootTags Array based on the comma delimited filter string passed in */
	void GetFilteredMessageRootTags(const FString& InFilterString, TArray< TSharedPtr<FMessageTagNode> >& OutTagArray) const;

	/** Returns "Categories" meta property from given handle, used for filtering by tag widget */
	FString GetCategoriesMetaFromPropertyHandle(TSharedPtr<class IPropertyHandle> PropertyHandle) const;

	/** Helper function, made to be called by custom OnGetCategoriesMetaFromPropertyHandle handlers  */
	static FString StaticGetCategoriesMetaFromPropertyHandle(TSharedPtr<class IPropertyHandle> PropertyHandle);

	/** Returns "Categories" meta property from given field, used for filtering by tag widget */
	template <typename TFieldType>
	FString GetCategoriesMetaFromField(TFieldType* Field) const
	{
		check(Field);
		if (Field->HasMetaData(NAME_Categories))
		{
			return Field->GetMetaData(NAME_Categories);
		}
		return FString();
	}

	/** Returns "Categories" meta property from given struct, used for filtering by tag widget */
	UE_DEPRECATED(4.22, "Please call GetCategoriesMetaFromField instead.")
	FString GetCategoriesMetaFromStruct(UScriptStruct* Struct) const { return GetCategoriesMetaFromField(Struct); }

	/** Returns "MessageTagFilter" meta property from given function, used for filtering by tag widget for any parameters of the function that end up as BP pins */
	FString GetCategoriesMetaFromFunction(const UFunction* Func, FName ParamName = NAME_None) const;

	/** Gets a list of all message tag nodes added by the specific source */
	void GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FMessageTagNode> >& OutTagArray) const;

	/** Returns true if this tag is directly in the dictionary already */
	bool IsDictionaryTag(FName TagName) const;

	/** Returns information about tag. If not found return false */
	bool GetTagEditorData(FName TagName, FString& OutComment, FName &OutTagSource, bool& bOutIsTagExplicit, bool &bOutIsRestrictedTag, bool &bOutAllowNonRestrictedChildren) const;

	/** Refresh the MessageTag tree due to an editor change */
	void EditorRefreshMessageTagTree();

	/** Gets a Tag Container containing all of the tags in the hierarchy that are children of this tag, and were explicitly added to the dictionary */
	FMessageTagContainer RequestMessageTagChildrenInDictionary(const FMessageTag& MessageTag) const;
#if WITH_EDITORONLY_DATA
	/** Gets a Tag Container containing all of the tags in the hierarchy that are children of this tag, were explicitly added to the dictionary, and do not have any explicitly added tags between them and the specified tag */
	FMessageTagContainer RequestMessageTagDirectDescendantsInDictionary(const FMessageTag& MessageTag, EMessageTagSelectionType SelectionType) const;
#endif // WITH_EDITORONLY_DATA

	/** This is called when EditorRefreshMessageTagTree. Useful if you need to do anything editor related when tags are added or removed */
	static FSimpleMulticastDelegate OnEditorRefreshMessageTagTree;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMessageTagDoubleClickedEditor, FMessageTag, FSimpleMulticastDelegate& /* OUT */)
	FOnMessageTagDoubleClickedEditor OnGatherMessageTagDoubleClickedEditor;

	/** Chance to dynamically change filter string based on a property handle */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGetCategoriesMetaFromPropertyHandle, TSharedPtr<IPropertyHandle>, FString& /* OUT */)
	FOnGetCategoriesMetaFromPropertyHandle OnGetCategoriesMetaFromPropertyHandle;

	/** Allows dynamic hiding of message tags in SMessageTagWidget. Allows higher order structs to dynamically change which tags are visible based on its own data */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFilterMessageTagChildren, const FString&  /** FilterString */, TSharedPtr<FMessageTagNode>& /* TagNode */, bool& /* OUT OutShouldHide */)
	FOnFilterMessageTagChildren OnFilterMessageTagChildren;

	struct FFilterMessageTagContext
	{
		const FString& FilterString;
		const TSharedPtr<FMessageTagNode>& TagNode;
		const FMessageTagSource* TagSource;
		const TSharedPtr<IPropertyHandle>& ReferencingPropertyHandle;

		FFilterMessageTagContext(const FString& InFilterString, const TSharedPtr<FMessageTagNode>& InTagNode, const FMessageTagSource* InTagSource, const TSharedPtr<IPropertyHandle>& InReferencingPropertyHandle)
			: FilterString(InFilterString), TagNode(InTagNode), TagSource(InTagSource), ReferencingPropertyHandle(InReferencingPropertyHandle)
		{}
	};
	/*
	 * Allows dynamic hiding of Message tags in SMessageTagWidget. Allows higher order structs to dynamically change which tags are visible based on its own data
	 * Applies to all tags, and has more context than OnFilterMessageTagChildren
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFilterMessageTag, const FFilterMessageTagContext& /** InContext */, bool& /* OUT OutShouldHide */)
	FOnFilterMessageTag OnFilterMessageTag;
	
	void NotifyMessageTagDoubleClickedEditor(FString TagName);
	
	bool ShowMessageTagAsHyperLinkEditor(FString TagName);


#endif //WITH_EDITOR

	void PrintReplicationIndices();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Mechanism for tracking what tags are frequently replicated */

	void PrintReplicationFrequencyReport();
	void NotifyTagReplicated(FMessageTag Tag, bool WasInContainer);

	TMap<FMessageTag, int32>	ReplicationCountMap;
	TMap<FMessageTag, int32>	ReplicationCountMap_SingleTags;
	TMap<FMessageTag, int32>	ReplicationCountMap_Containers;
#endif

private:

	/** Initializes the manager */
	static void InitializeManager();

	/** finished loading/adding native tags */
	static FSimpleMulticastDelegate& OnDoneAddingNativeTagsDelegate();

	/** The Tag Manager singleton */
	static UMessageTagsManager* SingletonManager;

	friend class FMessageTagTest;
	friend class FMessageEffectsTest;
	friend class FMessageTagsModule;
	friend class FMessageTagsEditorModule;
	friend class UMessageTagsSettings;
	friend class SAddNewMessageTagSourceWidget;
	friend class FNativeMessageTag;

	/**
	 * Helper function to insert a tag into a tag node array
	 *
	 * @param Tag							Short name of tag to insert
	 * @param FullTag						Full tag, passed in for performance
	 * @param ParentNode					Parent node, if any, for the tag
	 * @param NodeArray						Node array to insert the new node into, if necessary (if the tag already exists, no insertion will occur)
	 * @param SourceName					File tag was added from
	 * @param DevComment					Comment from developer about this tag
	 * @param bIsExplicitTag				Is the tag explicitly defined or is it implied by the existence of a child tag
	 * @param bIsRestrictedTag				Is the tag a restricted tag or a regular message tag
	 * @param bAllowNonRestrictedChildren	If the tag is a restricted tag, can it have regular message tag children or should all of its children be restricted tags as well?
	 *
	 * @return Index of the node of the tag
	 */
	int32 InsertTagIntoNodeArray(FName Tag, FName FullTag, TSharedPtr<FMessageTagNode> ParentNode, TArray< TSharedPtr<FMessageTagNode> >& NodeArray, FName SourceName, const FMessageTagTableRow& TagRow, bool bIsExplicitTag, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren);

	/** Helper function to populate the tag tree from each table */
	void PopulateTreeFromDataTable(class UDataTable* Table);

	void AddTagTableRow(const FMessageTagTableRow& TagRow, FName SourceName, bool bIsRestrictedTag = false, bool bAllowNonRestrictedChildren = true);

	void AddChildrenTags(FMessageTagContainer& TagContainer, TSharedPtr<FMessageTagNode> MessageTagNode, bool RecurseAll=true, bool OnlyIncludeDictionaryTags=false) const;

	void AddRestrictedMessageTagSource(const FString& FileName);

	void AddTagsFromAdditionalLooseIniFiles(const TArray<FString>& IniFileList);

	/**
	 * Helper function for MessageTagsMatch to get all parents when doing a parent match,
	 * NOTE: Must never be made public as it uses the FNames which should never be exposed
	 * 
	 * @param NameList		The list we are adding all parent complete names too
	 * @param MessageTag	The current Tag we are adding to the list
	 */
	void GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FMessageTagNode> MessageTag) const;

	/** Returns the tag source for a given tag source name, or null if not found */
	FMessageTagSource* FindOrAddTagSource(FName TagSourceName, EMessageTagSourceType SourceType, const FString& RootDirToUse= FString());

	/** Constructs the net indices for each tag */
	void ConstructNetIndex();

	/** Marks all of the nodes that descend from CurNode as having an ancestor node that has a source conflict. */
	void MarkChildrenOfNodeConflict(TSharedPtr<FMessageTagNode> CurNode);

	void VerifyNetworkIndex() const
	{
		if (bNetworkIndexInvalidated)
		{
			const_cast<UMessageTagsManager*>(this)->ConstructNetIndex();
		}
	}

	void InvalidateNetworkIndex() { bNetworkIndexInvalidated = true; }

	// Tag Sources
	///////////////////////////////////////////////////////

	/** These are the old native tags that use to be resisted via a function call with no specific site/ownership. */
	TSet<FName> LegacyNativeTags;

	/** Map of all config directories to load tag inis from */
	TMap<FString, FMessageTagSearchPathInfo> RegisteredSearchPaths;



	/** Roots of message tag nodes */
	TSharedPtr<FMessageTagNode> MessageRootTag;

	/** Map of Tags to Nodes - Internal use only. FMessageTag is inside node structure, do not use FindKey! */
	TMap<FMessageTag, TSharedPtr<FMessageTagNode>> MessageTagNodeMap;
	void SyncToGMPMeta();

	/** Our aggregated, sorted list of commonly replicated tags. These tags are given lower indices to ensure they replicate in the first bit segment. */
	TArray<FMessageTag> CommonlyReplicatedTags;

	/** Map of message tag source names to source objects */
	UPROPERTY()
	TMap<FName, FMessageTagSource> TagSources;

	TSet<FName> RestrictedMessageTagSourceNames;

	bool bIsConstructingMessageTagTree = false;

	/** Cached runtime value for whether we are using fast replication or not. Initialized from config setting. */
	bool bUseFastReplication;

	/** Cached runtime value for whether we should warn when loading invalid tags */
	bool bShouldWarnOnInvalidTags;

	/** Cached runtime value for whether we should warn when loading invalid tags */
	bool bShouldClearInvalidTags;

	/** True if native tags have all been added and flushed */
	bool bDoneAddingNativeTags;

	/** String with outlawed characters inside tags */
	FString InvalidTagCharacters;

#if WITH_EDITOR
	// This critical section is to handle an editor-only issue where tag requests come from another thread when async loading from a background thread in FMessageTagContainer::Serialize.
	// This class is not generically threadsafe.
	mutable FCriticalSection MessageTagMapCritical;

	// Transient editor-only tags to support quick-iteration PIE workflows
	TSet<FName> TransientEditorTags;
#endif

	/** Sorted list of nodes, used for network replication */
	TArray<TSharedPtr<FMessageTagNode>> NetworkMessageTagNodeIndex;

	uint32 NetworkMessageTagNodeIndexHash;

	bool bNetworkIndexInvalidated = true;

	/** Holds all of the valid message-related tags that can be applied to assets */
	UPROPERTY()
	TArray<UDataTable*> MessageTagTables;

	const static FName NAME_Categories;
	const static FName NAME_MessageTagFilter;
};
